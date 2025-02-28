/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2006  NEC Corporation
 *
 * Created by KaiGai Kohei <kaigai@ak.jp.nec.com>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

struct jffs2_acl_entry {
	jint16_t	e_tag;
	jint16_t	e_perm;
	jint32_t	e_id;
};

struct jffs2_acl_entry_short {
	jint16_t	e_tag;
	jint16_t	e_perm;
};

struct jffs2_acl_header {
	jint32_t	a_version;
};

#ifdef CONFIG_JFFS2_FS_POSIX_ACL

extern int jffs2_check_acl(struct inode *, int, unsigned int);
extern int jffs2_acl_chmod(struct inode *);
#if 0 // Jared, 04-04-2012,
// http://git.kernel.org/?p=linux/kernel/git/torvalds/linux-2.6.git;a=commit;h=963945bf93e46b9bf71a07bf9c78183e0f57733a
extern int jffs2_init_acl_pre(struct inode *, struct inode *, int *);
#else
extern int jffs2_init_acl_pre(struct inode *, struct inode *, mode_t *);
#endif
extern int jffs2_init_acl_post(struct inode *);

extern const struct xattr_handler jffs2_acl_access_xattr_handler;
extern const struct xattr_handler jffs2_acl_default_xattr_handler;

#else

#define jffs2_check_acl				(NULL)
#define jffs2_acl_chmod(inode)			(0)
#define jffs2_init_acl_pre(dir_i,inode,mode)	(0)
#define jffs2_init_acl_post(inode)		(0)

#endif	/* CONFIG_JFFS2_FS_POSIX_ACL */
