/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limUtils.cc contains the utility functions
 * LIM uses.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "aniGlobal.h"
#include "wniApi.h"

#include "sirCommon.h"
#if (WNI_POLARIS_FW_PRODUCT == AP)
#include "wniCfgAp.h"
#else
#include "wniCfgSta.h"
#endif
#include "cfgApi.h"

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halDataStruct.h"
#include "halCommonApi.h"
#endif

#include "utilsApi.h"
#include "limUtils.h"
#include "limSecurityUtils.h"
#include "limSession.h"


#define LIM_SEED_LENGTH 16

/**
 * limIsAuthAlgoSupported()
 *
 *FUNCTION:
 * This function is called in various places within LIM code
 * to determine whether passed authentication algorithm is enabled
 * or not
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param authType Indicates MAC based authentication type
 *                 (eSIR_OPEN_SYSTEM or eSIR_SHARED_KEY)
 *                 If Shared Key authentication to be used,
 *                 'Privacy Option Implemented' flag is also
 *                 checked.
 *
 * @return true if passed authType is enabled else false
 */
#ifdef WLAN_SOFTAP_FEATURE
tANI_U8
limIsAuthAlgoSupported(tpAniSirGlobal pMac, tAniAuthType authType, tpPESession psessionEntry)
#else
tANI_U8
limIsAuthAlgoSupported(tpAniSirGlobal pMac, tAniAuthType authType)
#endif
{
    tANI_U32 algoEnable, privacyOptImp;

    if (authType == eSIR_OPEN_SYSTEM)
    {

#ifdef WLAN_SOFTAP_FEATURE
        if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
        {
           if((psessionEntry->authType == eSIR_OPEN_SYSTEM) || (psessionEntry->authType == eSIR_AUTO_SWITCH))
              return true;
           else
              return false; 
        }
#endif

        if (wlan_cfgGetInt(pMac, WNI_CFG_OPEN_SYSTEM_AUTH_ENABLE,
                      &algoEnable) != eSIR_SUCCESS)
        {
            /**
             * Could not get AuthAlgo1 Enable value
             * from CFG. Log error.
               */
            limLog(pMac, LOGE,
                   FL("could not retrieve AuthAlgo1 Enable value\n"));

            return false;
        }
        else
            return ( (algoEnable > 0 ? true : false) );
    }
    else
    {

#ifdef WLAN_SOFTAP_FEATURE
        if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
        {
            if((psessionEntry->authType == eSIR_SHARED_KEY) || (psessionEntry->authType == eSIR_AUTO_SWITCH))
                algoEnable = true;
            else
                algoEnable = false;
            
        }
        else
#endif

        if (wlan_cfgGetInt(pMac, WNI_CFG_SHARED_KEY_AUTH_ENABLE,
                      &algoEnable) != eSIR_SUCCESS)
        {
            /**
             * Could not get AuthAlgo2 Enable value
             * from CFG. Log error.
             */
            limLog(pMac, LOGE,
                   FL("could not retrieve AuthAlgo2 Enable value\n"));

            return false;
        }

#ifdef WLAN_SOFTAP_FEATURE
        if(psessionEntry->limSystemRole == eLIM_AP_ROLE)
        {
            privacyOptImp = psessionEntry->privacy;
        }
        else
#endif

        if (wlan_cfgGetInt(pMac, WNI_CFG_PRIVACY_ENABLED,
                      &privacyOptImp) != eSIR_SUCCESS)
        {
            /**
             * Could not get PrivacyOptionImplemented value
             * from CFG. Log error.
             */
            limLog(pMac, LOGE,
               FL("could not retrieve PrivacyOptImplemented value\n"));

            return false;
        }
            return (algoEnable && privacyOptImp);
    }
} /****** end limIsAuthAlgoSupported() ******/



/**
 * limInitPreAuthList
 *
 *FUNCTION:
 * This function is called while starting a BSS at AP
 * to initialize MAC authenticated STA list. This may also be called
 * while joining/starting an IBSS if MAC authentication is allowed
 * in IBSS mode.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limInitPreAuthList(tpAniSirGlobal pMac)
{
    pMac->lim.pLimPreAuthList = NULL;

#if (WNI_POLARIS_FW_PRODUCT == AP)
    if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE )
    {
        tANI_U32 authClnupTimeout;
        //tANI_U32 cfgValue;

        if (wlan_cfgGetInt(pMac, WNI_CFG_PREAUTH_CLNUP_TIMEOUT,
                      &authClnupTimeout) != eSIR_SUCCESS)
        {
            /**
             * Could not get PreAuthClnupTimeout value
             * from CFG. Log error.
             */
            limLog(pMac, LOGE,
               FL("could not retrieve PreAuthClnupTimeout value\n"));

            return;
        }
        authClnupTimeout = SYS_MS_TO_TICKS(authClnupTimeout);

        /// Create and start periodic pre-auth context cleanup timeout
        if (tx_timer_create(&pMac->lim.limTimers.gLimPreAuthClnupTimer,
                            "preAuthCleanup",
                            limTimerHandler,
                            SIR_LIM_PREAUTH_CLNUP_TIMEOUT,
                            authClnupTimeout, authClnupTimeout,
                            TX_AUTO_ACTIVATE) != TX_SUCCESS)
        {
            /// Could not create PreAuthCleanup timer.
            // Log error
            limLog(pMac, LOGP, FL("could not create PreAuthCleanup timer\n"));

            return;
        }

#if defined(ANI_OS_TYPE_RTAI_LINUX)
        tx_timer_set_expiry_list(&pMac->lim.limTimers.gLimPreAuthClnupTimer,
                                 LIM_TIMER_EXPIRY_LIST);
#endif
        PELOG1(limLog(pMac, LOG1,
               FL("Created pre-auth cleanup timer\n"));)

    }
#endif
} /*** end limInitPreAuthList() ***/



/**
 * limDeletePreAuthList
 *
 *FUNCTION:
 * This function is called cleanup Pre-auth list either on
 * AP or on STA when moving from one persona to other.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limDeletePreAuthList(tpAniSirGlobal pMac)
{
    struct tLimPreAuthNode    *pCurrNode, *pTempNode;

    pCurrNode = pTempNode = pMac->lim.pLimPreAuthList;
    while (pCurrNode != NULL)
    {
        pTempNode = pCurrNode->next;

        PELOG1(limLog(pMac, LOG1, FL("=====> limDeletePreAuthList \n"));)
        limReleasePreAuthNode(pMac, pCurrNode);

        pCurrNode = pTempNode;
    }
    pMac->lim.pLimPreAuthList = NULL;
} /*** end limDeletePreAuthList() ***/



/**
 * limSearchPreAuthList
 *
 *FUNCTION:
 * This function is called when Authentication frame is received
 * by AP (or at a STA in IBSS supporting MAC based authentication)
 * to search if a STA is in the middle of MAC Authentication
 * transaction sequence.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  macAddr - MAC address of the STA that sent
 *                       Authentication frame.
 *
 * @return Pointer to pre-auth node if found, else NULL
 */

struct tLimPreAuthNode *
limSearchPreAuthList(tpAniSirGlobal pMac, tSirMacAddr macAddr)
{
    struct tLimPreAuthNode    *pTempNode = pMac->lim.pLimPreAuthList;

    while (pTempNode != NULL)
    {
        if (palEqualMemory( pMac->hHdd,(tANI_U8 *) macAddr,
                      (tANI_U8 *) &pTempNode->peerMacAddr,
                      sizeof(tSirMacAddr)) )
            break;

        pTempNode = pTempNode->next;
    }

    return pTempNode;
} /*** end limSearchPreAuthList() ***/



/**
 * limAddPreAuthNode
 *
 *FUNCTION:
 * This function is called at AP while sending Authentication
 * frame2.
 * This may also be called on a STA in IBSS if MAC authentication is
 * allowed in IBSS mode.
 *
 *LOGIC:
 * Node is always added to the front of the list
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pAuthNode - Pointer to pre-auth node to be added to the list.
 *
 * @return None
 */

void
limAddPreAuthNode(tpAniSirGlobal pMac, struct tLimPreAuthNode *pAuthNode)
{
    pMac->lim.gLimNumPreAuthContexts++;

    pAuthNode->next = pMac->lim.pLimPreAuthList;

    pMac->lim.pLimPreAuthList = pAuthNode;
} /*** end limAddPreAuthNode() ***/


/**
 * limReleasePreAuthNode
 *
 *FUNCTION:
 * This function is called to realease the accquired
 * pre auth node from list.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pAuthNode - Pointer to Pre Auth node to be released
 * @return None
 */

void
limReleasePreAuthNode(tpAniSirGlobal pMac, tpLimPreAuthNode pAuthNode)
{
    pAuthNode->fFree = 1;
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, 0, eLIM_PRE_AUTH_CLEANUP_TIMER));
    tx_timer_deactivate(&pAuthNode->timer);                
    pMac->lim.gLimNumPreAuthContexts--;
} /*** end limReleasePreAuthNode() ***/


/**
 * limDeletePreAuthNode
 *
 *FUNCTION:
 * This function is called at AP when a pre-authenticated STA is
 * Associated/Reassociated or when AuthFrame4 is received after
 * Auth Response timeout.
 * This may also be called on a STA in IBSS if MAC authentication and
 * Association/Reassociation is allowed in IBSS mode.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  peerMacAddr - MAC address of the STA that need to be deleted
 *                       from pre-auth node list.
 *
 * @return None
 */

void
limDeletePreAuthNode(tpAniSirGlobal pMac, tSirMacAddr macAddr)
{
    struct tLimPreAuthNode    *pPrevNode, *pTempNode;

    pTempNode = pPrevNode = pMac->lim.pLimPreAuthList;

    if (pTempNode == NULL)
        return;

    if (palEqualMemory( pMac->hHdd,(tANI_U8 *) macAddr,
                  (tANI_U8 *) &pTempNode->peerMacAddr,
                  sizeof(tSirMacAddr)) )
    {
        // First node to be deleted

        pMac->lim.pLimPreAuthList = pTempNode->next;

#if (WNI_POLARIS_FW_PRODUCT == AP)
        // Delete the auth response timer if running
        if (pTempNode->fTimerStarted)
            limDeactivateAndChangePerStaIdTimer(pMac,
                                                eLIM_AUTH_RSP_TIMER,
                                                pTempNode->authNodeIdx);

#endif

        PELOG1(limLog(pMac, LOG1, FL("=====> limDeletePreAuthNode : first node to delete\n"));)
        PELOG1(limLog(pMac, LOG1, FL("Release data entry: %x id %d peer \n"),
                        pTempNode, pTempNode->authNodeIdx);
        limPrintMacAddr(pMac, macAddr, LOG1);)
        limReleasePreAuthNode(pMac, pTempNode);

        return;
    }

    pTempNode = pTempNode->next;

    while (pTempNode != NULL)
    {
        if (palEqualMemory( pMac->hHdd,(tANI_U8 *) macAddr,
                      (tANI_U8 *) &pTempNode->peerMacAddr,
                      sizeof(tSirMacAddr)) )
        {
            // Found node to be deleted

            pPrevNode->next = pTempNode->next;

#if (WNI_POLARIS_FW_PRODUCT == AP)
            // Delete the auth response timer if running
            if (pTempNode->fTimerStarted)
                limDeactivateAndChangePerStaIdTimer(pMac,
                                                    eLIM_AUTH_RSP_TIMER,
                                                    pTempNode->authNodeIdx);
#endif
            PELOG1(limLog(pMac, LOG1, FL("=====> limDeletePreAuthNode : subsequent node to delete\n"));
            limLog(pMac, LOG1, FL("Release data entry: %x id %d peer \n"),
                         pTempNode, pTempNode->authNodeIdx);
            limPrintMacAddr(pMac, macAddr, LOG1);)
            limReleasePreAuthNode(pMac, pTempNode);

            return;
        }

        pPrevNode = pTempNode;
        pTempNode = pTempNode->next;
    }

    // Should not be here
    // Log error
    limLog(pMac, LOGP, FL("peer not found in pre-auth list, addr= "));
    limPrintMacAddr(pMac, macAddr, LOGP);

} /*** end limDeletePreAuthNode() ***/


#if (WNI_POLARIS_FW_PRODUCT == AP)
/**
 * limPreAuthClnupHandler
 *
 *FUNCTION:
 * This function is called on AP upon peridic Pre-authentication
 * context cleanup.
 *
 *LOGIC:
 * A Pre-auth node is marked as seen first time it comes across
 * the list traversal. It'll be deleted if already 'seen' (during
 * next Pre-auth cleanup).
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limPreAuthClnupHandler(tpAniSirGlobal pMac)
{
    tANI_U16              aid;
    tANI_U8             firstNode=false;
    tpDphHashNode    pStaDs;
    struct tLimPreAuthNode  *pPrevNode, *pCurrNode;

#ifdef GEN6_TODO
    //fetch the sessionEntry based on the sessionId
    //priority - MEDIUM
    tpPESession sessionEntry;

    if((sessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimPreAuthClnupTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
#endif

    pCurrNode = pPrevNode = pMac->lim.pLimPreAuthList;

    while (pCurrNode != NULL)
    {
        if (pCurrNode->fSeen)
        {
            // Found node to be deleted

            if (pCurrNode == pMac->lim.pLimPreAuthList)
            {
                // First node being deleted
                pMac->lim.pLimPreAuthList = pPrevNode = pCurrNode->next;
                firstNode = true;
            }
            else
            {
                pPrevNode->next = pCurrNode->next;
            }

            // Delete the auth response timer if running
            if (pCurrNode->fTimerStarted)
                limDeactivateAndChangePerStaIdTimer(pMac,
                                                    eLIM_AUTH_RSP_TIMER,
                                                    pCurrNode->authNodeIdx);

            pStaDs = dphLookupHashEntry(pMac,
                                        pCurrNode->peerMacAddr,
                                        &aid);

            if (!pStaDs)
            {
                /**
                 * STA does not have associated context.
                 * Send advisory Deauthentication frame
                 * to STA being deleted
                 */
                limSendDeauthMgmtFrame(
                               pMac,
                               eSIR_MAC_PREV_AUTH_NOT_VALID_REASON, //=2
                               pCurrNode->peerMacAddr,sessionEntry);
            }

            limLog(pMac,
                   LOG3,
                   FL("Release preAuth node during periodic cleanup\n"));
            limReleasePreAuthNode(pMac, pCurrNode);

            if (firstNode)
            {
                // First node was deleted
                if (pMac->lim.pLimPreAuthList == NULL)
                    break;

                pCurrNode = pMac->lim.pLimPreAuthList;
                firstNode = false;
            }
            else
            {
                pCurrNode = pPrevNode->next;
            }
        }
        else
        {
            // Mark this node as 'seen'. To be deleted next time.
            pCurrNode->fSeen = 1;

            pPrevNode = pCurrNode;
            pCurrNode = pCurrNode->next;
        }
    }
} /*** end limPreAuthClnupHandler() ***/
#endif



/**
 * limRestoreFromPreAuthState
 *
 *FUNCTION:
 * This function is called on STA whenever an Authentication
 * sequence is complete and state prior to auth need to be
 * restored.
 *
 *LOGIC:
 * MLM_AUTH_CNF is prepared and sent to SME state machine.
 * In case of restoring from pre-auth:
 *     - Channel Id is programmed at LO/RF synthesizer
 *     - BSSID is programmed at RHP
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac       - Pointer to Global MAC structure
 * @param  resultCode - result of authentication attempt
 * @return None
 */

void
limRestoreFromAuthState(tpAniSirGlobal pMac, tSirResultCodes resultCode, tANI_U16 protStatusCode,tpPESession sessionEntry)
{
    tSirMacAddr     currentBssId;
    tLimMlmAuthCnf  mlmAuthCnf;

    palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmAuthCnf.peerMacAddr,
                  (tANI_U8 *) &pMac->lim.gpLimMlmAuthReq->peerMacAddr,
                  sizeof(tSirMacAddr));
    mlmAuthCnf.authType   = pMac->lim.gpLimMlmAuthReq->authType;
    mlmAuthCnf.resultCode = resultCode;
    mlmAuthCnf.protStatusCode = protStatusCode;
    
    /* Update PE session ID*/
    mlmAuthCnf.sessionId = sessionEntry->peSessionId;

    /// Free up buffer allocated
    /// for pMac->lim.gLimMlmAuthReq
    palFreeMemory( pMac->hHdd, pMac->lim.gpLimMlmAuthReq);
    pMac->lim.gpLimMlmAuthReq = NULL;

    sessionEntry->limMlmState = sessionEntry->limPrevMlmState;
    
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));


    // 'Change' timer for future activations
    limDeactivateAndChangeTimer(pMac, eLIM_AUTH_FAIL_TIMER);

    #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) != eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,sessionEntry->bssId);

    if (sessionEntry->limSmeState == eLIM_SME_WT_PRE_AUTH_STATE)
    {
        pMac->lim.gLimPreAuthChannelNumber = 0;
    }

    limPostSmeMessage(pMac,
                      LIM_MLM_AUTH_CNF,
                      (tANI_U32 *) &mlmAuthCnf);
} /*** end limRestoreFromAuthState() ***/



/**
 * limLookUpKeyMappings()
 *
 *FUNCTION:
 * This function is called in limProcessAuthFrame() function
 * to determine if there exists a Key Mapping key for a given
 * MAC address.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param macAddr  MAC address of the peer STA for which existence
 *                 of Key Mapping key is to be determined
 *
 * @return pKeyMapEntry - Pointer to the keyMapEntry returned by CFG
 */

tCfgWepKeyEntry *
limLookUpKeyMappings(tSirMacAddr macAddr)
{
    return NULL;
} /****** end limLookUpKeyMappings() ******/



/**
 * limEncryptAuthFrame()
 *
 *FUNCTION:
 * This function is called in limProcessAuthFrame() function
 * to encrypt Authentication frame3 body.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac           Pointer to Global MAC structure
 * @param  keyId          key id to used
 * @param  pKey           Pointer to the key to be used for encryption
 * @param  pPlainText     Pointer to the body to be encrypted
 * @param  pEncrBody      Pointer to the encrypted auth frame body
 * @param  keyLength      8 (WEP40) or 16 (WEP104)
 * @return None
 */

void
limEncryptAuthFrame(tpAniSirGlobal pMac, tANI_U8 keyId, tANI_U8 *pKey, tANI_U8 *pPlainText,
                    tANI_U8 *pEncrBody, tANI_U32 keyLength)
{
    tANI_U8  seed[LIM_SEED_LENGTH], icv[SIR_MAC_WEP_ICV_LENGTH];

    keyLength += 3;

    // Bytes 0-2 of seed is IV
    // Read TSF timestamp into seed to get random IV - 1st 3 bytes
    halGetTxTSFtimer(pMac, (tSirMacTimeStamp *) &seed);

    // Bytes 3-7 of seed is key
    palCopyMemory( pMac->hHdd, (tANI_U8 *) &seed[3], pKey, keyLength - 3);

    // Compute CRC-32 and place them in last 4 bytes of plain text
    limComputeCrc32(icv, pPlainText, sizeof(tSirMacAuthFrameBody));

    palCopyMemory( pMac->hHdd, pPlainText + sizeof(tSirMacAuthFrameBody),
                  icv, SIR_MAC_WEP_ICV_LENGTH);

    // Run RC4 on plain text with the seed
    limRC4(pEncrBody + SIR_MAC_WEP_IV_LENGTH,
           (tANI_U8 *) pPlainText, seed, keyLength,
           LIM_ENCR_AUTH_BODY_LEN - SIR_MAC_WEP_IV_LENGTH);

    // Prepare IV
    pEncrBody[0] = seed[0];
    pEncrBody[1] = seed[1];
    pEncrBody[2] = seed[2];
    pEncrBody[3] = keyId << 6;
} /****** end limEncryptAuthFrame() ******/



/**
 * limComputeCrc32()
 *
 *FUNCTION:
 * This function is called to compute CRC-32 on a given source.
 * Used while encrypting/decrypting Authentication frame 3.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pDest    Destination location for computed CRC
 * @param  pSrc     Source location to be CRC computed
 * @param  len      Length over which CRC to be computed
 * @return None
 */

void
limComputeCrc32(tANI_U8 *pDest, tANI_U8 * pSrc, tANI_U8 len)
{
    tANI_U32 crc;
    int i;

    crc = 0;
    crc = ~crc;

    while(len-- > 0)
        crc = limCrcUpdate(crc, *pSrc++);

    crc = ~crc;

    for (i=0; i < SIR_MAC_WEP_IV_LENGTH; i++)
    {
        pDest[i] = (tANI_U8)crc;
        crc >>= 8;
    }
} /****** end limComputeCrc32() ******/



/**
 * limRC4()
 *
 *FUNCTION:
 * This function is called to run RC4 algorithm. Called while
 * encrypting/decrypting Authentication frame 3.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pDest          Destination location for encrypted text
 * @param  pSrc           Source location to be encrypted
 * @param  seed           Contains seed (IV + key) for PRNG
 * @param  keyLength      8 (WEP40) or 16 (WEP104)
 * @param  frameLen       Length of the frame
 *
 * @return None
 */

void
limRC4(tANI_U8 *pDest, tANI_U8 *pSrc, tANI_U8 *seed, tANI_U32 keyLength, tANI_U16 frameLen)
{
    typedef struct
    {
        tANI_U8 i, j;
        tANI_U8 sbox[256];
    } tRC4Context;

    tRC4Context ctx;

    {
        tANI_U16 i, j, k;

        //
        // Initialize sbox using seed
        //

        ctx.i = ctx.j = 0;
        for (i=0; i<256; i++)
            ctx.sbox[i] = (tANI_U8)i;

        j = 0;
        k = 0;
        for (i=0; i<256; i++)
        {
            tANI_U8 temp;

            j = (tANI_U8)(j + ctx.sbox[i] + seed[k]);
            temp = ctx.sbox[i];
            ctx.sbox[i] = ctx.sbox[j];
            ctx.sbox[j] = temp;

            if (++k >= keyLength)
                k = 0;
        }
    }

    {
        tANI_U8 i   = ctx.i;
        tANI_U8 j   = ctx.j;
        tANI_U8 len = (tANI_U8) frameLen;

        while (len-- > 0)
        {
            tANI_U8 temp1, temp2;

            i     = (tANI_U8)(i+1);
            temp1 = ctx.sbox[i];
            j     = (tANI_U8)(j + temp1);

            ctx.sbox[i] = temp2 = ctx.sbox[j];
            ctx.sbox[j] = temp1;

            temp1 = (tANI_U8)(temp1 + temp2);
            temp1 = ctx.sbox[temp1];
            temp2 = (tANI_U8)(pSrc ? *pSrc++ : 0);

            *pDest++ = (tANI_U8)(temp1 ^ temp2);
        }

        ctx.i = i;
        ctx.j = j;
    }
} /****** end limRC4() ******/



/**
 * limDecryptAuthFrame()
 *
 *FUNCTION:
 * This function is called in limProcessAuthFrame() function
 * to decrypt received Authentication frame3 body.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac       Pointer to Global MAC structure
 * @param pKey       Pointer to the key to be used for decryption
 * @param pEncrBody  Pointer to the body to be decrypted
 * @param pPlainBody Pointer to the decrypted body
 * @param keyLength  8 (WEP40) or 16 (WEP104)
 *
 * @return Decrypt result - eSIR_SUCCESS for success and
 *                          LIM_DECRYPT_ICV_FAIL for ICV mismatch.
 *                          If decryption is a success, pBody will
 *                          have decrypted auth frame body.
 */

tANI_U8
limDecryptAuthFrame(tpAniSirGlobal pMac, tANI_U8 *pKey, tANI_U8 *pEncrBody,
                    tANI_U8 *pPlainBody, tANI_U32 keyLength, tANI_U16 frameLen)
{
    tANI_U8  seed[LIM_SEED_LENGTH], icv[SIR_MAC_WEP_ICV_LENGTH];
    int i;
    keyLength += 3;


    // Bytes 0-2 of seed is received IV
    palCopyMemory( pMac->hHdd, (tANI_U8 *) seed, pEncrBody, SIR_MAC_WEP_IV_LENGTH - 1);

    // Bytes 3-7 of seed is key
    palCopyMemory( pMac->hHdd, (tANI_U8 *) &seed[3], pKey, keyLength - 3);

    // Run RC4 on encrypted text with the seed
    limRC4(pPlainBody,
           pEncrBody + SIR_MAC_WEP_IV_LENGTH,
           seed,
           keyLength,
           frameLen);

    PELOG4(limLog(pMac, LOG4, FL("plainbody is \n"));
    sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOG4, pPlainBody, frameLen);)

    // Compute CRC-32 and place them in last 4 bytes of encrypted body
    limComputeCrc32(icv,
                    (tANI_U8 *) pPlainBody,
                    (tANI_U8) (frameLen - SIR_MAC_WEP_ICV_LENGTH));

    // Compare RX_ICV with computed ICV
    for (i = 0; i < SIR_MAC_WEP_ICV_LENGTH; i++)
    {
       PELOG4(limLog(pMac, LOG4, FL(" computed ICV%d[%x], rxed ICV%d[%x]\n"),
               i, icv[i], i, pPlainBody[frameLen - SIR_MAC_WEP_ICV_LENGTH + i]);)
        if (icv[i] != pPlainBody[frameLen - SIR_MAC_WEP_ICV_LENGTH + i])
            return LIM_DECRYPT_ICV_FAIL;
    }

    return eSIR_SUCCESS;
} /****** end limDecryptAuthFrame() ******/

/**
 * limPostSmeSetKeysCnf
 *
 * A utility API to send MLM_SETKEYS_CNF to SME
 */
void limPostSmeSetKeysCnf( tpAniSirGlobal pMac,
    tLimMlmSetKeysReq *pMlmSetKeysReq,
    tLimMlmSetKeysCnf *mlmSetKeysCnf)
{
  // Prepare and Send LIM_MLM_SETKEYS_CNF
  palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmSetKeysCnf->peerMacAddr,
                (tANI_U8 *) pMlmSetKeysReq->peerMacAddr,
                sizeof(tSirMacAddr));

  palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmSetKeysCnf->peerMacAddr,
                (tANI_U8 *) pMlmSetKeysReq->peerMacAddr,
                sizeof(tSirMacAddr));

#if (WNI_POLARIS_FW_PRODUCT == AP)
  mlmSetKeysCnf->aid = pMlmSetKeysReq->aid;
#endif

  /// Free up buffer allocated for mlmSetKeysReq
  palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmSetKeysReq );
  pMac->lim.gpLimMlmSetKeysReq = NULL;

  limPostSmeMessage( pMac,
      LIM_MLM_SETKEYS_CNF,
      (tANI_U32 *) mlmSetKeysCnf );
}

/**
 * limPostSmeRemoveKeysCnf
 *
 * A utility API to send MLM_REMOVEKEY_CNF to SME
 */
void limPostSmeRemoveKeyCnf( tpAniSirGlobal pMac,
    tLimMlmRemoveKeyReq *pMlmRemoveKeyReq,
    tLimMlmRemoveKeyCnf *mlmRemoveKeyCnf)
{
  // Prepare and Send LIM_MLM_REMOVEKEYS_CNF
  palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmRemoveKeyCnf->peerMacAddr,
                (tANI_U8 *) pMlmRemoveKeyReq->peerMacAddr,
                sizeof(tSirMacAddr));

  /// Free up buffer allocated for mlmRemoveKeysReq
  palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmRemoveKeyReq );
  pMac->lim.gpLimMlmRemoveKeyReq = NULL;

  pMac->lim.gLimMlmState = pMac->lim.gLimPrevMlmState; //Restore the state.
  MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

  limPostSmeMessage( pMac,
      LIM_MLM_REMOVEKEY_CNF,
      (tANI_U32 *) mlmRemoveKeyCnf );
}

/**
 * limSendSetBssKeyReq()
 *
 *FUNCTION:
 * This function is called from limProcessMlmSetKeysReq(),
 * when PE is trying to setup the Group Keys related
 * to a specified encryption type
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac           Pointer to Global MAC structure
 * @param pMlmSetKeysReq Pointer to MLM_SETKEYS_REQ buffer
 * @return none
 */
void limSendSetBssKeyReq( tpAniSirGlobal pMac,
    tLimMlmSetKeysReq *pMlmSetKeysReq,
    tpPESession    psessionEntry)
{
tSirMsgQ           msgQ;
tpSetBssKeyParams  pSetBssKeyParams = NULL;
tLimMlmSetKeysCnf  mlmSetKeysCnf;
tSirRetStatus      retCode;
tANI_U32 val = 0;

  if(pMlmSetKeysReq->numKeys > SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS)
  {
      limLog( pMac, LOG1,
          FL( "numKeys = %d is more than SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS\n" ), pMlmSetKeysReq->numKeys);
      
      // Respond to SME with error code
      mlmSetKeysCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
      goto end;
  }

  // Package WDA_SET_BSSKEY_REQ message parameters

  if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
          (void **) &pSetBssKeyParams,
         sizeof( tSetBssKeyParams )))     
  {
    limLog( pMac, LOGE,
        FL( "Unable to PAL allocate memory during SET_BSSKEY\n" ));

    // Respond to SME with error code
    mlmSetKeysCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
    goto end;
  }
  else
    palZeroMemory( pMac->hHdd,
        (void *) pSetBssKeyParams,
         sizeof( tSetBssKeyParams ));     

  // Update the WDA_SET_BSSKEY_REQ parameters
  pSetBssKeyParams->bssIdx = psessionEntry->bssIdx;
  pSetBssKeyParams->encType = pMlmSetKeysReq->edType;
  pSetBssKeyParams->numKeys = pMlmSetKeysReq->numKeys;

  if(eSIR_SUCCESS != wlan_cfgGetInt(pMac, WNI_CFG_SINGLE_TID_RC, &val))
  {
     limLog( pMac, LOGP, FL( "Unable to read WNI_CFG_SINGLE_TID_RC\n" ));
  }

  pSetBssKeyParams->singleTidRc = (tANI_U8)val;

  /* Update PE session Id*/
  pSetBssKeyParams->sessionId = psessionEntry ->peSessionId;

  palCopyMemory( pMac->hHdd,
      (tANI_U8 *) &pSetBssKeyParams->key,
      (tANI_U8 *) &pMlmSetKeysReq->key,
      sizeof( tSirKeys ) * pMlmSetKeysReq->numKeys );

  SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
  msgQ.type = WDA_SET_BSSKEY_REQ;
  //
  // FIXME_GEN4
  // A global counter (dialog token) is required to keep track of
  // all PE <-> HAL communication(s)
  //
  msgQ.reserved = 0;
  msgQ.bodyptr = pSetBssKeyParams;
  msgQ.bodyval = 0;

  limLog( pMac, LOGW,
      FL( "Sending WDA_SET_BSSKEY_REQ...\n" ));
  MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));
  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
  {
    limLog( pMac, LOGE,
        FL("Posting SET_BSSKEY to HAL failed, reason=%X\n"),
        retCode );

    // Respond to SME with LIM_MLM_SETKEYS_CNF
    mlmSetKeysCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
  }
  else
    return; // Continue after WDA_SET_BSSKEY_RSP...

end:
  limPostSmeSetKeysCnf( pMac,
      pMlmSetKeysReq,
      &mlmSetKeysCnf );

}

/**
 * @function : limSendSetStaKeyReq()
 *
 * @brief :  This function is called from limProcessMlmSetKeysReq(),
 * when PE is trying to setup the Unicast Keys related
 * to a specified STA with specified encryption type
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac           Pointer to Global MAC structure
 * @param pMlmSetKeysReq Pointer to MLM_SETKEYS_REQ buffer
 * @param staIdx         STA index for which the keys are being set
 * @param defWEPIdx      The default WEP key index [0..3]
 * @return none
 */
void limSendSetStaKeyReq( tpAniSirGlobal pMac,
    tLimMlmSetKeysReq *pMlmSetKeysReq,
    tANI_U16 staIdx,
    tANI_U8 defWEPIdx,
    tpPESession sessionEntry)
{
tSirMsgQ           msgQ;
tpSetStaKeyParams  pSetStaKeyParams = NULL;
tLimMlmSetKeysCnf  mlmSetKeysCnf;
tSirRetStatus      retCode;
tANI_U32 val = 0;

  // Package WDA_SET_STAKEY_REQ message parameters
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **) &pSetStaKeyParams,
                                                                                                 sizeof( tSetStaKeyParams ))) {
        limLog( pMac, LOGP, FL( "Unable to PAL allocate memory during SET_BSSKEY\n" ));
        return;
    }else
        palZeroMemory( pMac->hHdd, (void *) pSetStaKeyParams, sizeof( tSetStaKeyParams ));

  // Update the WDA_SET_STAKEY_REQ parameters
  pSetStaKeyParams->staIdx = staIdx;
  pSetStaKeyParams->encType = pMlmSetKeysReq->edType;

  
  if(eSIR_SUCCESS != wlan_cfgGetInt(pMac, WNI_CFG_SINGLE_TID_RC, &val))
  {
     limLog( pMac, LOGP, FL( "Unable to read WNI_CFG_SINGLE_TID_RC\n" ));
  }

  pSetStaKeyParams->singleTidRc = (tANI_U8)val;

  /* Update  PE session ID*/
  pSetStaKeyParams->sessionId = sessionEntry->peSessionId;

  /**
   * For WEP - defWEPIdx indicates the default WEP
   * Key to be used for TX
   * For all others, there's just one key that can
   * be used and hence it is assumed that
   * defWEPIdx = 0 (from the caller)
   */

  pSetStaKeyParams->defWEPIdx = defWEPIdx;
    
  /** Store the Previous MlmState*/
  sessionEntry->limPrevMlmState = sessionEntry->limMlmState;
  SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    
  if(sessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE && !pMlmSetKeysReq->key[0].unicast) {
      sessionEntry->limMlmState = eLIM_MLM_WT_SET_STA_BCASTKEY_STATE;
      msgQ.type = WDA_SET_STA_BCASTKEY_REQ;
  }else {
      sessionEntry->limMlmState = eLIM_MLM_WT_SET_STA_KEY_STATE;
      msgQ.type = WDA_SET_STAKEY_REQ;
  }
  MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

  /**
   * In the Case of WEP_DYNAMIC, ED_TKIP and ED_CCMP
   * the Key[0] contains the KEY, so just copy that alone,
   * for the case of WEP_STATIC the hal gets the key from cfg
   */
  switch( pMlmSetKeysReq->edType ) {
  case eSIR_ED_WEP40:
  case eSIR_ED_WEP104:
      // FIXME! Is this OK?
      if( 0 == pMlmSetKeysReq->numKeys ) {
#ifdef WLAN_SOFTAP_FEATURE
          tANI_U32 i;

          for(i=0; i < SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS ;i++)
          { 
              palCopyMemory( pMac->hHdd,
                             (tANI_U8 *) &pSetStaKeyParams->key[i],
                             (tANI_U8 *) &pMlmSetKeysReq->key[i], sizeof( tSirKeys ));
          }
#endif
          pSetStaKeyParams->wepType = eSIR_WEP_STATIC;
          sessionEntry->limMlmState = eLIM_MLM_WT_SET_STA_KEY_STATE;
          MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
      }else {
          pSetStaKeyParams->wepType = eSIR_WEP_DYNAMIC;
          palCopyMemory( pMac->hHdd,
                         (tANI_U8 *) &pSetStaKeyParams->key,
                         (tANI_U8 *) &pMlmSetKeysReq->key[0], sizeof( tSirKeys ));
      }
      break;
  case eSIR_ED_TKIP:
  case eSIR_ED_CCMP:
#ifdef FEATURE_WLAN_WAPI 
  case eSIR_ED_WPI: 
#endif
      {
          palCopyMemory( pMac->hHdd, (tANI_U8 *) &pSetStaKeyParams->key,
                         (tANI_U8 *) &pMlmSetKeysReq->key[0], sizeof( tSirKeys ));
      }
      break;
  default:
      break;
  }

  
  //
  // FIXME_GEN4
  // A global counter (dialog token) is required to keep track of
  // all PE <-> HAL communication(s)
  //
  msgQ.reserved = 0;
  msgQ.bodyptr = pSetStaKeyParams;
  msgQ.bodyval = 0;

  limLog( pMac, LOG1, FL( "Sending WDA_SET_STAKEY_REQ...\n" ));
  MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));
  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ ))) {
      limLog( pMac, LOGE, FL("Posting SET_STAKEY to HAL failed, reason=%X\n"), retCode );
      // Respond to SME with LIM_MLM_SETKEYS_CNF
      mlmSetKeysCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
  }else
      return; // Continue after WDA_SET_STAKEY_RSP...

  limPostSmeSetKeysCnf( pMac, pMlmSetKeysReq, &mlmSetKeysCnf );
}

/**
 * limSendRemoveBssKeyReq()
 *
 *FUNCTION:
 * This function is called from limProcessMlmRemoveReq(),
 * when PE is trying to Remove a Group Key related
 * to a specified encryption type
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac           Pointer to Global MAC structure
 * @param pMlmRemoveKeyReq Pointer to MLM_REMOVEKEY_REQ buffer
 * @return none
 */
void limSendRemoveBssKeyReq( tpAniSirGlobal pMac,
    tLimMlmRemoveKeyReq *pMlmRemoveKeyReq,
    tpPESession   psessionEntry)
{
tSirMsgQ           msgQ;
tpRemoveBssKeyParams  pRemoveBssKeyParams = NULL;
tLimMlmRemoveKeyCnf  mlmRemoveKeysCnf;
tSirRetStatus      retCode;

  // Package WDA_REMOVE_BSSKEY_REQ message parameters

  if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
          (void **) &pRemoveBssKeyParams,
         sizeof( tRemoveBssKeyParams )))     
  {
    limLog( pMac, LOGE,
        FL( "Unable to PAL allocate memory during REMOVE_BSSKEY\n" ));

    // Respond to SME with error code
    mlmRemoveKeysCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
    goto end;
  }
  else
    palZeroMemory( pMac->hHdd,
        (void *) pRemoveBssKeyParams,
         sizeof( tRemoveBssKeyParams ));     

  // Update the WDA_REMOVE_BSSKEY_REQ parameters
  pRemoveBssKeyParams->bssIdx = psessionEntry->bssIdx;
  pRemoveBssKeyParams->encType = pMlmRemoveKeyReq->edType;
  pRemoveBssKeyParams->keyId = pMlmRemoveKeyReq->keyId;
  pRemoveBssKeyParams->wepType = pMlmRemoveKeyReq->wepType;

  /* Update PE session Id*/

  pRemoveBssKeyParams->sessionId = psessionEntry->peSessionId;

  msgQ.type = WDA_REMOVE_BSSKEY_REQ;
  //
  // FIXME_GEN4
  // A global counter (dialog token) is required to keep track of
  // all PE <-> HAL communication(s)
  //
  msgQ.reserved = 0;
  msgQ.bodyptr = pRemoveBssKeyParams;
  msgQ.bodyval = 0;

  limLog( pMac, LOGW,
      FL( "Sending WDA_REMOVE_BSSKEY_REQ...\n" ));
  MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));

  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
  {
    limLog( pMac, LOGE,
        FL("Posting REMOVE_BSSKEY to HAL failed, reason=%X\n"),
        retCode );

    // Respond to SME with LIM_MLM_REMOVEKEYS_CNF
    mlmRemoveKeysCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
  }
  else
    return; 

end:
  limPostSmeRemoveKeyCnf( pMac,
      pMlmRemoveKeyReq,
      &mlmRemoveKeysCnf );

}

/**
 * limSendRemoveStaKeyReq()
 *
 *FUNCTION:
 * This function is called from limProcessMlmRemoveKeysReq(),
 * when PE is trying to setup the Unicast Keys related
 * to a specified STA with specified encryption type
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param pMac           Pointer to Global MAC structure
 * @param pMlmRemoveKeysReq Pointer to MLM_REMOVEKEYS_REQ buffer
 * @param staIdx         STA index for which the keys are being set
 * @return none
 */
void limSendRemoveStaKeyReq( tpAniSirGlobal pMac,
    tLimMlmRemoveKeyReq *pMlmRemoveKeyReq,
    tANI_U16 staIdx ,
    tpPESession sessionEntry)
{
tSirMsgQ           msgQ;
tpRemoveStaKeyParams  pRemoveStaKeyParams = NULL;
tLimMlmRemoveKeyCnf  mlmRemoveKeyCnf;
tSirRetStatus      retCode;



  if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
          (void **) &pRemoveStaKeyParams,
          sizeof( tRemoveStaKeyParams )))
  {
    limLog( pMac, LOGE,
        FL( "Unable to PAL allocate memory during REMOVE_STAKEY\n" ));

    // Respond to SME with error code
    mlmRemoveKeyCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
    goto end;
  }
  else
    palZeroMemory( pMac->hHdd,
        (void *) pRemoveStaKeyParams,
        sizeof( tRemoveStaKeyParams ));

  if( (pMlmRemoveKeyReq->edType == eSIR_ED_WEP104 || pMlmRemoveKeyReq->edType == eSIR_ED_WEP40) &&
        pMlmRemoveKeyReq->wepType == eSIR_WEP_STATIC )
  {
        PELOGE(limLog(pMac, LOGE, FL("Request to remove static WEP keys through station interface\n Should use BSS interface\n"));)
        mlmRemoveKeyCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
        goto end;
  }
 
  // Update the WDA_REMOVEKEY_REQ parameters
  pRemoveStaKeyParams->staIdx = staIdx;
  pRemoveStaKeyParams->encType = pMlmRemoveKeyReq->edType;
  pRemoveStaKeyParams->keyId = pMlmRemoveKeyReq->keyId;
  pRemoveStaKeyParams->unicast = pMlmRemoveKeyReq->unicast;

  /* Update PE session ID*/
  pRemoveStaKeyParams->sessionId = sessionEntry->peSessionId;

  SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
  
  msgQ.type = WDA_REMOVE_STAKEY_REQ;
  //
  // FIXME_GEN4
  // A global counter (dialog token) is required to keep track of
  // all PE <-> HAL communication(s)
  //
  msgQ.reserved = 0;
  msgQ.bodyptr = pRemoveStaKeyParams;
  msgQ.bodyval = 0;

  limLog( pMac, LOGW,
      FL( "Sending WDA_REMOVE_STAKEY_REQ...\n" ));
  MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));
  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
  {
    limLog( pMac, LOGE,
        FL("Posting REMOVE_STAKEY to HAL failed, reason=%X\n"),
        retCode );

    // Respond to SME with LIM_MLM_REMOVEKEY_CNF
    mlmRemoveKeyCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
  }
  else
    return; 

end:
  limPostSmeRemoveKeyCnf( pMac,
      pMlmRemoveKeyReq,
      &mlmRemoveKeyCnf );

}

