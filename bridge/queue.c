/* 
** Enduro/X Queue processing (ATMI/NDRXD queues).
** Addtional processing:
** Internal message queue used for cases when target
** service queues are full. So that if we try to send, we do not
** block the whole bridge, but instead we leave the message in internal linked
** list and try later. If tries are exceeded time-out value, then we just drop
** the message.
**
** @file queue.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <utlist.h>

#include <ndebug.h>
#include <atmi.h>
#include <atmi_int.h>
#include <atmi_shm.h>
#include <typed_buf.h>
#include <ndrstandard.h>
#include <ubf.h>
#include <Exfields.h>
#include <gencall.h>

#include <exnet.h>
#include <ndrxdcmn.h>
#include "bridge.h"
#include "../libatmisrv/srv_int.h"
#include "userlog.h"
/*---------------------------Externs------------------------------------*/
/*---------------------------Macros-------------------------------------*/

#define     PACK_TYPE_TONDRXD           1           /* Send message NDRXD                   */
#define     PACK_TYPE_TOSVC             2           /* Send to service, use timer (their)   */
#define     PACK_TYPE_TORPLYQ           3           /* Send to reply q, use timer (internal)*/

/*---------------------------Enums--------------------------------------*/
/*---------------------------Typedefs-----------------------------------*/
/*---------------------------Globals------------------------------------*/
in_msg_t *M_in_q = NULL;            /* Linked list with incoming message in Q */
/*---------------------------Statics------------------------------------*/
/*---------------------------Prototypes---------------------------------*/


/**
 * Enqueue the message for delayed send.
 * @param call
 * @param len
 * @param from_q
 * @return 
 */
private int br_add_to_q(char *buf, int len, int pack_type)
{
    int ret=SUCCEED;
    in_msg_t *msg;
    
    if (NULL==(msg=calloc(1, sizeof(in_msg_t))))
    {
        NDRX_ERR_MALLOC(sizeof(in_msg_t));
        FAIL_OUT(ret);
    }
    
    /*fill in the details*/
    msg->pack_type = pack_type;
    msg->len = len;
    memcpy(msg->buffer, buf, len);
    
    ndrx_timer_reset(&msg->trytime);
    
    DL_APPEND(M_in_q, msg);
    
out:
    return ret;
}


/**
 * Generate error to network if required.
 * We detect the packet type here.
 * @param buf
 * @param len
 * @param pack_type
 * @return 
 */
private int br_generate_error_to_net(char *buf, int len, int pack_type, long rcode)
{
    int ret=SUCCEED;
    
    switch(pack_type)
    {
        case PACK_TYPE_TONDRXD:
            /* Will not generate any error here*/
            break;
        case PACK_TYPE_TOSVC:
            /* So This was TPCALL, we might want to send error reply back. */
        {
            tp_command_call_t *call = (tp_command_call_t *)buf;
            
            if (!(call->flags & TPNOREPLY))
            {
                NDRX_LOG(log_warn, "Sending back error reply");
                reply_with_failure(TPNOBLOCK, call, NULL, NULL, rcode);
            }
        }
            break;
        case PACK_TYPE_TORPLYQ:
            /* Will not generate any error here*/
            break;
        default:
            NDRX_LOG(log_warn, "Nothing to do for pack_type=%d", 
                    pack_type);
            break;
    }
    
out:
    return ret;
}

/**
 * So we got q send error.
 */
private int br_process_error(char *buf, int len, int err, in_msg_t* from_q, int pack_type)
{
    long rcode = TPESVCERR;
    
    if (err==ENOENT)
    {
        rcode = TPENOENT;
    }
    
    /* So this is initial call */
    if (NULL==from_q)
    {
        /* If error is EAGAIN, then enqueue the message */
        if (EAGAIN==err)
        {
            /* Put stuff in queue */
            br_add_to_q(buf, len, pack_type);
        }
        else
        {
            /* TODO: Ignore the error or send error back - TPNOENT probably (if this is request) */
            br_generate_error_to_net(buf, len, pack_type, rcode);
        }
    }
    else
    {
        NDRX_LOG(log_debug, "Got error processing from Q");
        /* In this case we should handle the stuff!?!?
         * Generate reply (only if needed), Not sure TPNOTENT or svcerr?
         * SVCERR could be better because initially we thought that service exists.
         */
        br_generate_error_to_net(buf, len, pack_type, rcode);
        if (EAGAIN!=err)
        {
            /* Generate error reply */
            DL_DELETE(M_in_q, from_q);
            free(from_q);
        }
    }
    
    return SUCCEED;
}

/**
 * Send stuff directly to NDRXD
 */
public int br_submit_to_ndrxd(command_call_t *call, int len, in_msg_t* from_q)
{
    int ret=SUCCEED;
    
    if (SUCCEED!=(ret=generic_q_send(ndrx_get_G_atmi_conf()->ndrxd_q_str, 
            (char *)call, len, TPNOBLOCK)))
    {
        NDRX_LOG(log_error, "Failed to send message to ndrxd!");
        br_process_error((char *)call, len, ret, from_q, PACK_TYPE_TONDRXD);
    }
    
out:
    return SUCCEED;    
}

/**
 * Submit to service. We should do this via Q
 */
public int br_submit_to_service(tp_command_call_t *call, int len, in_msg_t* from_q)
{
    int ret=SUCCEED;
    char svc_q[NDRX_MAX_Q_SIZE+1];
    int is_bridge = FALSE;
    
    if (ATMI_COMMAND_EVPOST==call->command_id)
    {
        if (SUCCEED!=_get_evpost_sendq(svc_q, call->extradata))
        {
            NDRX_LOG(log_error, "Failed figure out postage Q");
            ret=FAIL;
            goto out;
        }
    }
    else
    {
        /* Resolve the service in SHM 
         *   sprintf(svc_q, NDRX_SVC_QFMT, G_server_conf.q_prefix, call->name); */
                                                                                
        if (SUCCEED!=ndrx_shm_get_svc(call->name, svc_q, &is_bridge))
        {
            NDRX_LOG(log_error, "Failed to get local service [%s] for bridge call!",
                    call->name);
            userlog("Failed to get local service [%s] for bridge call!", call->name);
            br_process_error((char *)call, len, ret, from_q, PACK_TYPE_TOSVC);
            FAIL_OUT(ret);
        }
    }
    
    NDRX_LOG(log_debug, "Calling service: %s", svc_q);
    if (SUCCEED!=(ret=generic_q_send(svc_q, (char *)call, len, TPNOBLOCK)))
    {
        NDRX_LOG(log_error, "Failed to send message to ndrxd!");
        br_process_error((char *)call, len, ret, from_q, PACK_TYPE_TOSVC);
    }
    /* TODO: Check the result, if called failed, then reply back with error? */
    
out:
    return SUCCEED;    
}

/**
 * Submit reply to service. We should do this via Q
 */
public int br_submit_reply_to_q(tp_command_call_t *call, int len, in_msg_t* from_q)
{
    char reply_to[NDRX_MAX_Q_SIZE+1];
    int ret=SUCCEED;
    /* TODO: We have problem here, because of missing reply_to */
    if (!from_q)
    {
        if (SUCCEED!=fill_reply_queue(call->callstack, call->reply_to, reply_to))
        {
            NDRX_LOG(log_error, "Failed to send message to ndrxd!");
            goto out;
        }
    }
    
    NDRX_LOG(log_debug, "Reply to Q: %s", reply_to);
    if (SUCCEED!=(ret=generic_q_send(reply_to, (char *)call, len, TPNOBLOCK)))
    {
        NDRX_LOG(log_error, "Failed to send message to %s!", reply_to);
        br_process_error((char *)call, len, ret, from_q, PACK_TYPE_TORPLYQ);
        goto out;
    }
    
out:
    return SUCCEED;    
}



/**
 * At this point we got message from Q, so we should forward it to network.
 * @param buf
 * @param len
 * @param msg_type
 * @return 
 */
public int br_got_message_from_q(char *buf, int len, char msg_type)
{
    int ret=SUCCEED;
    
    
    NDRX_DUMP(log_debug, "Got message from Q:", buf, len);
    
    if (msg_type==BR_NET_CALL_MSG_TYPE_ATMI)
    {
        tp_command_generic_t *gen_command = (tp_command_generic_t *)buf;
        NDRX_LOG(log_debug, "Got from Q ATMI message: %d", 
                gen_command->command_id);
        
        switch (gen_command->command_id)
        {

            case ATMI_COMMAND_TPCALL:
            case ATMI_COMMAND_CONNECT:
                
                NDRX_LOG(log_debug, "TPCALL/CONNECT from Q");
                /* Adjust the clock */
                br_clock_adj((tp_command_call_t *)buf, TRUE);
                /* Send stuff to network, adjust clock.*/
                ret=br_send_to_net(buf, len, BR_NET_CALL_MSG_TYPE_ATMI, 
                        gen_command->command_id);
                
                if (SUCCEED!=ret)
                {
                    /* Generate TPNOENT */
                }
                
                break;
                
            /* tpreply & conversation goes via reply Q */
            case ATMI_COMMAND_TPREPLY:
            case ATMI_COMMAND_CONVDATA:
            case ATMI_COMMAND_CONNRPLY:
            case ATMI_COMMAND_DISCONN:
            case ATMI_COMMAND_CONNUNSOL:
            case ATMI_COMMAND_CONVACK:
            case ATMI_COMMAND_SHUTDOWN:
            case ATMI_COMMAND_EVPOST:
                
                NDRX_LOG(log_debug, "TPREPLY/CONVERSATION from Q");
                
                /* Adjust the clock */
                br_clock_adj((tp_command_call_t *)buf, TRUE);
                
                ret=br_send_to_net(buf, len, BR_NET_CALL_MSG_TYPE_ATMI, 
                        gen_command->command_id);
                
                if (SUCCEED!=ret)
                {
                    NDRX_LOG(log_error, "Failed to send reply to "
                            "net - nothing todo");
                    ret=SUCCEED;
                }
               
                break;
            case ATMI_COMMAND_TPFORWARD:
                /* not used */
                break;
            case ATMI_COMMAND_SELF_SD:
                G_shutdown_nr_got++;
            
                NDRX_LOG(log_warn, "Got shutdown req %d of %d", 
                        G_shutdown_nr_got, G_shutdown_nr_wait);

                break;
        }
        
    }
    else if (msg_type==BR_NET_CALL_MSG_TYPE_NDRXD)
    {
        command_call_t *call = (command_call_t *)buf;
        NDRX_LOG(log_debug, "Got from Q NDRXD message");
        /* I guess we can forward these directly but firstly check the type
         * we do not want to send any spam to other machine, do we?
         * Hmm but lets try out?
         */
        if (SUCCEED!=br_send_to_net(buf, len, msg_type, call->command))
        {
            FAIL_OUT(ret);
        }
    }
out:
    return ret;
}

/**
 * Process any stuff we have in queue!
 * @return 
 */
public int br_run_queue(void)
{
    int ret=SUCCEED;
    char *fn="br_run_queue";
    in_msg_t *elt = NULL;
    in_msg_t *tmp = NULL;
    
    NDRX_LOG(log_debug, "%s - enter", fn);
    
    DL_FOREACH_SAFE(M_in_q, elt, tmp)
    {
        /* Check the time-out
         * TODO: Support for TPNOTIME!!!!
         */
        if (ndrx_timer_get_delta_sec(&elt->trytime) >=G_atmi_env.time_out)
        {
            NDRX_LOG(log_warn, "Dropping message of type %d due to "
                    "time-out condition!", elt->pack_type);
            DL_DELETE(M_in_q, elt);
            free(elt);
        }
        else
        {
            /* Process it only not sure about error handling... generally we ignore the error */
            switch (elt->pack_type)
            {
                /* WARNING!!!! POSSIBLE MEM LEAK HERE!!!! */
                case PACK_TYPE_TONDRXD:
                    br_submit_to_ndrxd((command_call_t *)elt->buffer, elt->len, elt);
                    break;
                case PACK_TYPE_TOSVC: 
                    br_submit_to_service((tp_command_call_t *)elt->buffer, elt->len, elt);
                    break;
                case PACK_TYPE_TORPLYQ:
                    br_submit_reply_to_q((tp_command_call_t *)elt->buffer, elt->len, elt);
                    break;
                default:
                    NDRX_LOG(log_error, "Unknown queued message type %d",
                            elt->pack_type);
                    FAIL_OUT(ret);
                    break;
            }
        }
    }
    
out:    
    NDRX_LOG(log_debug, "%s - return", fn);
    
    return ret;
}

