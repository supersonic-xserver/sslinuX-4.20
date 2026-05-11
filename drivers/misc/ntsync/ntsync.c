// SPDX-License-Identifier: GPL-2.0
/*
 * ntsync.c - NT synchronization primitive driver
 *
 * A support driver for emulation of NT synchronization primitives
 * by user-space NT emulators (Wine, Proton). This provides:
 * - Semaphores (counting)
 * - Mutexes (recursive with owner tracking)
 * - Events (auto-reset and manual-reset)
 *
 * Based on: https://docs.kernel.org/next/userspace-api/ntsync.html
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#include <uapi/linux/ntsync.h>

/* Maximum handles in a single wait */
#define NTSYNC_MAX_WAIT_COUNT	64

/* Object types */
enum ntsync_type {
	NTSYNC_OBJ_SEM,
	NTSYNC_OBJ_MUTEX,
	NTSYNC_OBJ_EVENT,
};

/* Object flags */
#define NTSYNC_FLAG_SIGNALED	0x01
#define NTSYNC_FLAG_AUTO	0x02	/* Auto-reset event */

/**
 * struct ntsync_obj - Base synchronization object
 */
struct ntsync_obj {
	enum ntsync_type type;
	struct kref kref;
	union {
		struct {
			__u32 count;
			__u32 max;
		} sem;
		struct {
			__u32 owner;
			__u32 count;	/* Recursion count */
		} mutex;
		struct {
			__u8 manual;
		} event;
	};
	wait_queue_head_t wait;
};

/**
 * struct ntsync_file - Per-file context
 */
struct ntsync_file {
	struct idr object_idr;
	struct mutex lock;
	wait_queue_head_t wait_all;
};

/* Object allocation/management - uses per-file IDR for object management */

static struct ntsync_obj *ntsync_obj_alloc(enum ntsync_type type)
{
	struct ntsync_obj *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return NULL;

	kref_init(&obj->kref);
	obj->type = type;
	init_waitqueue_head(&obj->wait);

	return obj;
}

static inline void ntsync_obj_get(struct ntsync_obj *obj)
{
	kref_get(&obj->kref);
}

static void ntsync_obj_free(struct kref *kref)
{
	struct ntsync_obj *obj = container_of(kref, struct ntsync_obj, kref);
	kfree(obj);
}

static inline void ntsync_obj_put(struct ntsync_obj *obj)
{
	kref_put(&obj->kref, ntsync_obj_free);
}

/* Semaphore operations */
static int ntsync_sem_init(struct ntsync_obj *sem, __u32 count, __u32 max)
{
	if (count > max)
		return -EINVAL;

	sem->type = NTSYNC_OBJ_SEM;
	sem->sem.count = count;
	sem->sem.max = max;

	return 0;
}

static __u32 ntsync_sem_post(struct ntsync_obj *sem, __u32 prev)
{
	__u32 ret = sem->sem.count;
	if (sem->sem.count < sem->sem.max)
		sem->sem.count++;
	else
		ret = 0;  /* Max reached */
	return ret;
}

/* Mutex operations */
static int ntsync_mutex_init(struct ntsync_obj *mutex, __u32 owner)
{
	mutex->type = NTSYNC_OBJ_MUTEX;
	mutex->mutex.owner = owner;
	mutex->mutex.count = 1;

	return 0;
}

static __u32 ntsync_mutex_unlock(struct ntsync_obj *mutex, __u32 owner)
{
	if (mutex->mutex.owner != owner)
		return 0;  /* Not owned */

	mutex->mutex.count--;
	if (mutex->mutex.count == 0) {
		mutex->mutex.owner = 0;
		return 1;  /* Successfully released */
	}
	return 0;
}

/* Event operations */
static int ntsync_event_init(struct ntsync_obj *event, __u32 manual, __u32 signaled)
{
	event->type = NTSYNC_OBJ_EVENT;
	event->event.manual = manual;

	if (signaled)
		set_bit(0, (unsigned long *)&event->type);

	return 0;
}

static void ntsync_event_signal(struct ntsync_obj *event)
{
	if (test_and_set_bit(0, (unsigned long *)&event->type))
		return;  /* Already signaled */

	if (!event->event.manual) {
		/* Auto-reset: wake one and clear */
		wake_up(&event->wait);
		clear_bit(0, (unsigned long *)&event->type);
	} else {
		/* Manual-reset: wake all */
		wake_up_all(&event->wait);
	}
}

static void ntsync_event_reset(struct ntsync_obj *event)
{
	clear_bit(0, (unsigned long *)&event->type);
}

/* Device file operations */
static int ntsync_get_object(struct ntsync_file *file, unsigned int cmd,
			     void __user *argp, struct ntsync_obj **obj)
{
	struct ntsync_obj *tmp;
	int id;

	switch (cmd) {
	case NTSYNC_IOC_CREATE_SEM: {
		struct ntsync_sem_args args;
		if (copy_from_user(&args, argp, sizeof(args)))
			return -EFAULT;

		tmp = ntsync_obj_alloc(NTSYNC_OBJ_SEM);
		if (!tmp)
			return -ENOMEM;

		ntsync_sem_init(tmp, args.initial, args.max);
		ntsync_obj_get(tmp);
		id = idr_alloc(&file->object_idr, tmp, 1, 0, GFP_KERNEL);
		if (id < 0) {
			ntsync_obj_put(tmp);
			return id;
		}

		args.initial = tmp->sem.count;
		args.max = tmp->sem.max;
		if (copy_to_user(argp, &args, sizeof(args)))
			return -EFAULT;

		return id;
	}

	case NTSYNC_IOC_CREATE_MUTEX: {
		struct ntsync_mutex_args args;
		if (copy_from_user(&args, argp, sizeof(args)))
			return -EFAULT;

		tmp = ntsync_obj_alloc(NTSYNC_OBJ_MUTEX);
		if (!tmp)
			return -ENOMEM;

		ntsync_mutex_init(tmp, args.owner);
		id = idr_alloc(&file->object_idr, tmp, 1, 0, GFP_KERNEL);
		if (id < 0) {
			ntsync_obj_put(tmp);
			return id;
		}

		args.owner = tmp->mutex.owner;
		if (copy_to_user(argp, &args, sizeof(args)))
			return -EFAULT;

		return id;
	}

	case NTSYNC_IOC_CREATE_EVENT: {
		struct ntsync_event_args args;
		if (copy_from_user(&args, argp, sizeof(args)))
			return -EFAULT;

		tmp = ntsync_obj_alloc(NTSYNC_OBJ_EVENT);
		if (!tmp)
			return -ENOMEM;

		ntsync_event_init(tmp, args.manual_reset, args.signaled);
		id = idr_alloc(&file->object_idr, tmp, 1, 0, GFP_KERNEL);
		if (id < 0) {
			ntsync_obj_put(tmp);
			return id;
		}

		args.signaled = test_bit(0, (unsigned long *)&tmp->type);
		if (copy_to_user(argp, &args, sizeof(args)))
			return -EFAULT;

		return id;
	}

	default:
		return -ENOTTY;
	}
}

static int ntsync_qry_object(struct ntsync_file *file, unsigned int cmd,
			     void __user *argp)
{
	struct ntsync_qry_args args;
	struct ntsync_obj *obj;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;

	obj = idr_find(&file->object_idr, args.owner);
	if (!obj)
		return -EINVAL;

	args.owner = 0;
	args.count = 0;
	args.signaled = 0;

	switch (obj->type) {
	case NTSYNC_OBJ_SEM:
		args.count = obj->sem.count;
		args.signaled = (obj->sem.count > 0);
		break;

	case NTSYNC_OBJ_MUTEX:
		args.owner = obj->mutex.owner;
		args.signaled = (obj->mutex.owner == 0);
		break;

	case NTSYNC_OBJ_EVENT:
		args.signaled = test_bit(0, (unsigned long *)&obj->type);
		break;
	}

	return copy_to_user(argp, &args, sizeof(args)) ? -EFAULT : 0;
}

static int ntsync_wait_objects(struct ntsync_file *file,
			       struct ntsync_wait_args __user *argp,
			       int wait_all)
{
	struct ntsync_wait_args args;
	struct ntsync_obj *objects[NTSYNC_MAX_WAIT_COUNT];
	ktime_t timeout;
	int i, ret = 0;
	int j;

	if (copy_from_user(&args, argp, sizeof(args)))
		return -EFAULT;

	if (args.count == 0 || args.count > NTSYNC_MAX_WAIT_COUNT)
		return -EINVAL;

	/* Get objects */
	mutex_lock(&file->lock);
	for (i = 0; i < args.count; i++) {
		objects[i] = idr_find(&file->object_idr, i);
		if (!objects[i]) {
			mutex_unlock(&file->lock);
			return -EINVAL;
		}
		ntsync_obj_get(objects[i]);
	}
	mutex_unlock(&file->lock);

	if (args.timeout == NTSYNC_INFINITE)
		timeout = KTIME_MAX;
	else
		timeout = ns_to_ktime(args.timeout);

	/* Wait */
	ret = wait_event_interruptible_timeout(
		file->wait_all,
		({
			bool signaled = false;

			for (j = 0; j < args.count; j++) {
				struct ntsync_obj *obj = objects[j];
				switch (obj->type) {
				case NTSYNC_OBJ_SEM:
					signaled = (obj->sem.count > 0);
					break;
				case NTSYNC_OBJ_MUTEX:
					signaled = (obj->mutex.owner == 0);
					break;
				case NTSYNC_OBJ_EVENT:
					signaled = test_bit(0, (unsigned long *)&obj->type);
					break;
				}
				if (signaled && !wait_all)
					break;
			}

			signaled;
		}),
		args.timeout == NTSYNC_INFINITE ? MAX_SCHEDULE_TIMEOUT :
			nsecs_to_jiffies(args.timeout)
	);

	/* Release references */
	for (i = 0; i < args.count; i++)
		ntsync_obj_put(objects[i]);

	if (ret == -ERESTARTSYS)
		return -EINTR;

	args.first = 0;
	args.signaled = 0;

	return copy_to_user(argp, &args, sizeof(args)) ? -EFAULT : 0;
}

static long ntsync_file_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	struct ntsync_file *f = file->private_data;
	void __user *argp = (void __user *)arg;
	struct ntsync_obj *obj;
	__u32 id;
	__u32 prev;
	int ret;

	switch (cmd) {
	case NTSYNC_IOC_CREATE_SEM:
	case NTSYNC_IOC_CREATE_MUTEX:
	case NTSYNC_IOC_CREATE_EVENT:
		return ntsync_get_object(f, cmd, argp, NULL);

	case NTSYNC_IOC_WAIT_ANY:
		return ntsync_wait_objects(f, argp, 0);

	case NTSYNC_IOC_WAIT_ALL:
		return ntsync_wait_objects(f, argp, 1);

	case NTSYNC_IOC_QRY_SEM:
	case NTSYNC_IOC_QRY_MUTEX:
	case NTSYNC_IOC_QRY_EVENT:
		return ntsync_qry_object(f, cmd, argp);

	case NTSYNC_IOC_SEM_POST:
		if (copy_from_user(&id, argp, sizeof(id)))
			return -EFAULT;
		mutex_lock(&f->lock);
		obj = idr_find(&f->object_idr, id);
		if (!obj || obj->type != NTSYNC_OBJ_SEM) {
			mutex_unlock(&f->lock);
			return -EINVAL;
		}
		prev = ntsync_sem_post(obj, 0);
		mutex_unlock(&f->lock);
		if (put_user(prev, (__u32 __user *)argp))
			return -EFAULT;
		return 0;

	case NTSYNC_IOC_MUTEX_UNLOCK:
		if (copy_from_user(&id, argp, sizeof(id)))
			return -EFAULT;
		mutex_lock(&f->lock);
		obj = idr_find(&f->object_idr, id);
		if (!obj || obj->type != NTSYNC_OBJ_MUTEX) {
			mutex_unlock(&f->lock);
			return -EINVAL;
		}
		ret = ntsync_mutex_unlock(obj, id);
		mutex_unlock(&f->lock);
		return ret;

	case NTSYNC_IOC_EVENT_SET:
		if (copy_from_user(&id, argp, sizeof(id)))
			return -EFAULT;
		mutex_lock(&f->lock);
		obj = idr_find(&f->object_idr, id);
		if (!obj || obj->type != NTSYNC_OBJ_EVENT) {
			mutex_unlock(&f->lock);
			return -EINVAL;
		}
		ntsync_event_signal(obj);
		mutex_unlock(&f->lock);
		return 0;

	case NTSYNC_IOC_EVENT_RESET:
		if (copy_from_user(&id, argp, sizeof(id)))
			return -EFAULT;
		mutex_lock(&f->lock);
		obj = idr_find(&f->object_idr, id);
		if (!obj || obj->type != NTSYNC_OBJ_EVENT) {
			mutex_unlock(&f->lock);
			return -EINVAL;
		}
		ntsync_event_reset(obj);
		mutex_unlock(&f->lock);
		return 0;

	case NTSYNC_IOC_EVENT_PULSE:
		if (copy_from_user(&id, argp, sizeof(id)))
			return -EFAULT;
		mutex_lock(&f->lock);
		obj = idr_find(&f->object_idr, id);
		if (!obj || obj->type != NTSYNC_OBJ_EVENT) {
			mutex_unlock(&f->lock);
			return -EINVAL;
		}
		ntsync_event_signal(obj);
		ntsync_event_reset(obj);
		mutex_unlock(&f->lock);
		return 0;

	case NTSYNC_IOC_DELETE:
		idr_destroy(&f->object_idr);
		return 0;

	default:
		return -ENOTTY;
	}
}

static __poll_t ntsync_file_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	struct ntsync_file *f = file->private_data;
	struct ntsync_obj *obj;
	int id;
	__poll_t mask = 0;

	poll_wait(file, &f->wait_all, wait);

	/* Poll for signaled objects */
	mutex_lock(&f->lock);

	idr_for_each_entry(&f->object_idr, obj, id) {
		switch (obj->type) {
		case NTSYNC_OBJ_SEM:
			if (obj->sem.count > 0)
				mask |= EPOLLIN;
			break;
		case NTSYNC_OBJ_MUTEX:
			if (obj->mutex.owner == 0)
				mask |= EPOLLIN;
			break;
		case NTSYNC_OBJ_EVENT:
			if (test_bit(0, (unsigned long *)&obj->type))
				mask |= EPOLLIN;
			break;
		}
	}

	mutex_unlock(&f->lock);

	return mask;
}

static int ntsync_file_release(struct inode *inode, struct file *file)
{
	struct ntsync_file *f = file->private_data;
	struct ntsync_obj *obj;
	int id;

	idr_for_each_entry(&f->object_idr, obj, id) {
		ntsync_obj_put(obj);
	}
	idr_destroy(&f->object_idr);
	kfree(f);

	return 0;
}

static int ntsync_file_open(struct inode *inode, struct file *file)
{
	struct ntsync_file *f;

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;

	idr_init(&f->object_idr);
	mutex_init(&f->lock);
	init_waitqueue_head(&f->wait_all);

	file->private_data = f;

	return 0;
}

static const struct file_operations ntsync_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ntsync_file_ioctl,
	.compat_ioctl = ntsync_file_ioctl,
	.poll = ntsync_file_poll,
	.open = ntsync_file_open,
	.release = ntsync_file_release,
};

static struct miscdevice ntsync_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ntsync",
	.fops = &ntsync_fops,
	.mode = 0666,
};

static int __init ntsync_init(void)
{
	int ret;

	ret = misc_register(&ntsync_misc);
	if (ret) {
		pr_err("ntsync: failed to register misc device: %d\n", ret);
		return ret;
	}

	pr_info("ntsync: NT synchronization primitives driver initialized\n");
	return 0;
}

static void __exit ntsync_exit(void)
{
	misc_deregister(&ntsync_misc);
	pr_info("ntsync: driver unloaded\n");
}

module_init(ntsync_init);
module_exit(ntsync_exit);

MODULE_DESCRIPTION("NT synchronization primitive driver (Wine/Proton support)");
MODULE_AUTHOR("sslinuX-4.20");
MODULE_LICENSE("GPL v2");