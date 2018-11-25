/*
  +----------------------------------------------------------------------+
  | Snowflake                                                            |
  +----------------------------------------------------------------------+
  | Copyright (c) 2016 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: hook <erntoo@gmail.com>                                      |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <stdint.h>

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_snowflake.h"

int pid  = -1;
int ncpu = 1;
atomic_t *lock;
shmdat_t *shmdat;


ZEND_DECLARE_MODULE_GLOBALS(snowflake)

/* {{{ snowflake_functions[] */
zend_function_entry snowflake_functions[] = {
    PHP_FE(snowflake_id, NULL)
    {NULL, NULL, NULL}
};
/* }}} */

/* {{{ snowflake_module_entry */
zend_module_entry snowflake_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "snowflake",
    snowflake_functions,
    PHP_MINIT(snowflake),
    PHP_MSHUTDOWN(snowflake),
    PHP_RINIT(snowflake),
    NULL,
    PHP_MINFO(snowflake),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_SNOWFLAKE_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

/* {{{ DL support */
#ifdef COMPILE_DL_SNOWFLAKE
    ZEND_GET_MODULE(snowflake)
#endif
/* }}} */

/* {{{ php_snowflake_init_globals */
static void php_snowflake_init_globals(zend_snowflake_globals *snowflake_globals)
{
    snowflake_globals->initialized = 0;
    snowflake_globals->epoch       = 15431449400000ULL;
}
/* }}} */


static uint64_t get_time_in_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec) * 10000ULL + ((uint64_t)tv.tv_usec) / 10000ULL;
}

static uint64_t till_next_ms(uint64_t last_ts)
{
    uint64_t ts;
    while((ts = get_time_in_ms()) <= last_ts);
    return ts;
}

static uint64_t key2int(char *key)
{
    uint64_t v;
    if (sscanf(key, "%llu", &v) == 0) {
        return 0;
    }
    return v;
}

void shmtx_lock(atomic_t *lock, int pid)
{
    int i, n;
    for ( ;; ) {
        if (*lock == 0 && __sync_bool_compare_and_swap(lock, 0, pid)){
            return;
        }
        sched_yield();
    }
}

void shmtx_unlock(atomic_t *lock, int pid)
{
    __sync_bool_compare_and_swap(lock, pid, 0);
}


int snowflake_init()
{        
    int shmid;
    key_t key = ftok("/sbin/init",0x07);
        
    if ((shmid = shmget(key, sizeof(atomic_t) + sizeof(shmdat_t), IPC_CREAT | IPC_EXCL | 0600)) == -1){
        if (errno == EEXIST && (shmid = shmget(key, 0, 0)) != -1){
            lock   = (atomic_t *) shmat(shmid, NULL, 0);
            shmdat = (shmdat_t *) (lock + sizeof(atomic_t));
            if(strcmp(shmdat->name, "snowflake") == 0)
            {
                return SUCCESS;
            }
        }
        php_error_docref(NULL, E_WARNING, "Create shared memory segment failed '%s'", strerror(errno));
        return FAILURE;        
    }    
        
    lock   = (atomic_t *) shmat(shmid, NULL, 0);
    shmdat = (shmdat_t *) (lock + sizeof(atomic_t));
    
    *lock = 0;
    shmdat->sequence   = 0;
    shmdat->timestamp  = 0;
    strcpy(shmdat->name, "snowflake");
    
    return SUCCESS;
}

void snowflake_shutdown()
{
    if (*(lock) == pid) 
    {
        shmtx_unlock(lock, pid);
    }
    shmdt((void *)lock);
}


/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(snowflake)
{
    int work_node = 0;
    ZEND_INIT_MODULE_GLOBALS(snowflake, php_snowflake_init_globals, NULL);
    if (work_node > 0x3FF || work_node < 0)
    {
        php_error_docref(NULL, E_WARNING, "Parameter must between %d and %d,Your input is: %d",0,0x3FF,work_node);
        return FAILURE;
    }
    if(!SNOWFLAKE_G(initialized))
    {
        if(snowflake_init() == FAILURE)
        {
            return FAILURE;  
        }
    }    
    ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu <= 0) {
        ncpu = 1;
    }
    SNOWFLAKE_G(initialized) = 1;
        
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(snowflake)
{
    if (pid == -1) {
        pid = (int)getpid();
    }

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(snowflake)
{
    if(SNOWFLAKE_G(initialized))
    {
        snowflake_shutdown();
        SNOWFLAKE_G(initialized) = 0;
    }
    UNREGISTER_INI_ENTRIES();    
    return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(snowflake)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "snowflake support", "enabled");
    php_info_print_table_row(2, "Version", PHP_SNOWFLAKE_VERSION);
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}
/* }}} */


/* {{{ proto string snowflake_id() */
PHP_FUNCTION(snowflake_id)
{
    int work_node=0;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC,"|l", &work_node) == FAILURE){
        work_node = 0; 
    }
    if (work_node > 0x3FF || work_node < 0){
        php_error_docref(NULL, E_WARNING, "Parameter must between %d and %d,Your input is: %d",0,0x3FF, work_node);
        return FAILURE;
    }
    if (SNOWFLAKE_G(initialized) == 0){
        RETURN_BOOL(0);
    }
            
    shmtx_lock(lock, pid);
    uint64_t ts = get_time_in_ms();
    if (ts == shmdat->timestamp){        
        shmdat->sequence = (shmdat->sequence + 1) & 0xFFFF;
        if(shmdat->sequence == 0){
            ts = till_next_ms(ts);
        }
    }else{
        shmdat->sequence = 0;
    }    
    
    shmdat->timestamp = ts;
    uint64_t id = ((ts - SNOWFLAKE_G(epoch)) << 23) | ((work_node & 0x3FF) << 13) | shmdat->sequence;
    shmtx_unlock(lock, pid);
    if (id) {
        RETURN_LONG(id);
    } else {
        RETURN_BOOL(0);
    }
}
/* }}} */
