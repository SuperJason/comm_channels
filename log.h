#ifndef LOGS_H__
#define LOGS_H__

#ifndef LOG_TAGS 
#define LOG_TAGS "DEBUG"
#endif

#ifdef DEBUG
#define pr_debug(fmt, ...) printf(" [" LOG_TAGS "] " fmt, ##__VA_ARGS__)
#else /* DEBUG */
#define pr_debug(fmt, ...)
#endif /* DEBUG */

#define pr_err(fmt, ...) printf(" [" LOG_TAGS "] " fmt, ##__VA_ARGS__)

#define pr_notice(fmt, ...) printf(" [" LOG_TAGS "] " fmt, ##__VA_ARGS__)

#endif /* LOGS_H__ */
