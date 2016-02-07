/* 
** Client Process Monitor Server
**
** @file cpmsrv.h
** 
** -----------------------------------------------------------------------------
** Enduro/X Middleware Platform for Distributed Transaction Processing
** Copyright (C) 2015, ATR Baltic, SIA. All Rights Reserved.
** This software is released under one of the following licenses:
** GPL or ATR Baltic's license for commercial use.
** -----------------------------------------------------------------------------
** GPL license:
** 
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; either version 2 of the License, or (at your option) any later
** version.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY
** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
** PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 59 Temple
** Place, Suite 330, Boston, MA 02111-1307 USA
**
** -----------------------------------------------------------------------------
** A commercial use license is available from ATR Baltic, SIA
** contact@atrbaltic.com
** -----------------------------------------------------------------------------
*/

#ifndef CPMSRV_H
#define	CPMSRV_H

#ifdef	__cplusplus
extern "C" {
#endif

/*---------------------------Includes-----------------------------------*/
#include <uthash.h>
#include <ntimer.h>
#include <cpm.h>
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/

/* Process flags */
#define CPM_F_AUTO_START          1      /* bit for auto start                      */
#define CPM_F_EXTENSIVE_CHECK     2     /* do extensive checks for process existance */
    
    
#define NDRX_CLTTAG                 "NDRX_CLTTAG" /* Tag format string         */
#define NDRX_CLTSUBSECT             "NDRX_CLTSUBSECT" /* Subsect format string */
    
#define S_FS                        0x1c /* Field seperator */
    
    
#define CLT_STATE_NOTRUN            0   /* Not running                  */
#define CLT_STATE_STARTING          1   /* Starting...                  */
#define CLT_STATE_STARTED           2   /* Started                      */
    
#define CLT_CHK_INTERVAL_DEFAULT    15  /* Do the checks every 15 sec   */
#define CLT_KILL_INTERVAL_DEFAULT    30  /* Default kill interval       */
    
/* Individual shutdown: */
#define CLT_STEP_INTERVAL           10000 /*  microseconds for usleep */
#define CLT_STEP_SECOND              CLT_STEP_INTERVAL / 1000000.0f /* part of second */
    
/* Global shutdown: */
#define CLT_STEP_INTERVAL_ALL          300000 /*  microseconds for usleep */
#define CLT_STEP_SECOND_ALL            CLT_STEP_INTERVAL / 1000000.0f /* part of second */
/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
    
    
/**
 * Configuration info
 */
struct cpm_static_info
{
    /* Static info: */
    char command_line[PATH_MAX+1+CPM_TAG_LEN+CPM_SUBSECT_LEN];
    char log_stdout[PATH_MAX+1];
    char log_stderr[PATH_MAX+1];
    char env[PATH_MAX+1];
    long flags;              /* flags of the process */
};
typedef struct cpm_static_info cpm_static_info_t;


/**
 * Runtime info, pids, statuses, times.
 */
struct cpm_dynamic_info
{
    /* Dynamic info: */
    pid_t pid;  /* pid of the process */
    int exit_status; /* Exit status... */
    int consecutive_fail; /*  Number of consecutive fails */    
    int req_state;      /* requested state */
    int cur_state;      /* current state */
    int was_started;    /* Was process started? */
    time_t stattime;         /* Status change time */
};
typedef struct cpm_dynamic_info cpm_dynamic_info_t;

/**
 * Definition of client processes (full command line & all settings)
 */
struct cpm_process
{   
    char key[CPM_KEY_LEN+1]; /* tag<FS>subsect */
    
    char tag[CPM_TAG_LEN+1];
    char subsect[CPM_SUBSECT_LEN+1];
    
    cpm_static_info_t stat;
    cpm_dynamic_info_t dyn;
    int is_cfg_refresh; /* Is config refreshed? */

    UT_hash_handle hh;         /* makes this structure hashable */
};
typedef struct cpm_process cpm_process_t;


/**
 * Definition of client processes (full command line & all settings)
 */
struct cpmsrv_config
{
    char *config_file;
    short chk_interval; /* Interval for process checking... */
    short kill_interval; /* Signalling interval */
};
typedef struct cpmsrv_config cpmsrv_config_t;

/*---------------------------Globals------------------------------------*/
extern cpmsrv_config_t G_config;
extern cpm_process_t *G_clt_config;
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/
extern int load_config(void);
extern cpm_process_t * cpm_get_client_by_pid(pid_t pid);
extern cpm_process_t * cpm_client_get(char *tag, char *subsect);
extern void sign_chld_handler(int sig);
extern int cpm_kill(cpm_process_t *c);
extern cpm_process_t * cpm_start_all(void);
#ifdef	__cplusplus
}
#endif

#endif	/* CPMSRV_H */
