#!/usr/bin/env python3
"""
Replace stub halTxUpdateCutThroughDesc with real cut-through token buffer assembly.

Usage: python3 patch_cutthru.py [path/to/hal_pdma.c]
Default path: os/linux/hif/common/hal_pdma.c
"""
import sys
import re

path = sys.argv[1] if len(sys.argv) > 1 else "os/linux/hif/common/hal_pdma.c"

OLD = """\
void halTxUpdateCutThroughDesc(struct GLUE_INFO *prGlueInfo,
                               struct MSDU_INFO *prMsduInfo,
                               struct MSDU_TOKEN_ENTRY *prFillToken,
                               struct MSDU_TOKEN_ENTRY *prToken,
                               uint32_t u4Idx,
                               bool fgIsLast)
{
    uint32_t *p32;
    size_t desc_len = 0;
    uint32_t u4FrameLen = 0;

    /* Basic guards */
    if (!prGlueInfo) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL prGlueInfo\\n");
        return;
    }
    if (!prGlueInfo->prAdapter) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL prAdapter\\n");
        return;
    }
    if (!prMsduInfo) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL prMsduInfo\\n");
        return;
    }
    if (!prFillToken || !prToken) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL token(s)\\n");
        return;
    }
    if (!prToken->prPacket) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: prToken->prPacket NULL (token=%u)\\n",
               prToken->u4Token);
        return;
    }

    /* Validate frame length */
    u4FrameLen = prMsduInfo->u2FrameLength;
    if (u4FrameLen == 0 || u4FrameLen > 4096) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: bad frame len=%u\\n", u4FrameLen);
        return;
    }

    /* Determine descriptor buffer we can safely write into.
     * Many callers expect prMsduInfo->aucTxDescBuffer to contain the TXD template.
     * Use that buffer if present; if not present, bail (we must not write-through token internals).
     */
    /* If the struct contains aucTxDescBuffer, sizeof(...) will compile; otherwise adapt locally. */
#ifdef HAVE_MSDU_AUC_TX_DESC_BUFFER
    desc_len = sizeof(prMsduInfo->aucTxDescBuffer);
    if (desc_len == 0) {
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: aucTxDescBuffer length 0\\n");
        return;
    }
    /* zero the descriptor template to avoid leaking stale data */
    kalMemZero(prMsduInfo->aucTxDescBuffer, desc_len);
    p32 = (uint32_t *)prMsduInfo->aucTxDescBuffer;
#else
    /*
     * If your tree does not define HAVE_MSDU_AUC_TX_DESC_BUFFER, we still try to be conservative:
     * attempt to use the prMsduInfo->aucTxDescBuffer symbol if it exists, otherwise abort.
     * You can remove this block if aucTxDescBuffer is always present in your build.
     */
    #if defined(ARRAY_SIZE) /* fallback - try to detect */
        desc_len = sizeof(prMsduInfo->aucTxDescBuffer);
        if (desc_len == 0) {
            DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: no aucTxDescBuffer\\n");
            return;
        }
        kalMemZero(prMsduInfo->aucTxDescBuffer, desc_len);
        p32 = (uint32_t *)prMsduInfo->aucTxDescBuffer;
    #else
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: aucTxDescBuffer not available in this build\\n");
        return;
    #endif
#endif

    /* Minimal, portable descriptor population:
     * place a few conservative values that downstream code can inspect:
     *  - word 0 : frame length (lower 16 bits)
     *  - word 1 : token id (u4Token)
     *  - word 2 : index/flags (idx in upper half, last flag low bit)
     *
     * This is intentionally generic: replace with platform-specific field sets later.
     */
    if (desc_len >= (3 * sizeof(uint32_t))) {
        p32[0] = ((uint32_t)u4FrameLen & 0x0000FFFFU);
        p32[1] = prToken->u4Token;
        p32[2] = ((u4Idx & 0xFFFFU) << 16) | (fgIsLast ? 1U : 0U);
    } else {
        /* Descriptor area too small; fail closed */
        DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: descriptor buffer too small (%zu)\\n", desc_len);
        return;
    }

    /* Debug print so we can detect when upstream provides bad tokens */
    DBGLOG(HAL, INFO,
           "halTxUpdateCutThroughDesc: token=%u fillToken=%u len=%u idx=%u last=%u desc=%p",
           prToken->u4Token, prFillToken->u4Token, u4FrameLen, u4Idx, fgIsLast, (void *)p32);

    /* NOTE: We intentionally avoid writing into token-specific descriptor pointers
     * (e.g. prFillToken->prToken) because that member is not present in all trees.
     * The safe approach: prepare the TXD in prMsduInfo->aucTxDescBuffer and let
     * the later "fill ring" code copy that template into the DMA descriptor if/when available.
     */
}"""

NEW = """\
void halTxUpdateCutThroughDesc(struct GLUE_INFO *prGlueInfo,
			       struct MSDU_INFO *prMsduInfo,
			       struct MSDU_TOKEN_ENTRY *prFillToken,
			       struct MSDU_TOKEN_ENTRY *prToken,
			       uint32_t u4Idx,
			       bool fgIsLast)
{
	struct ADAPTER *prAdapter;
	struct mt66xx_chip_info *prChipInfo;
	struct sk_buff *prSkb;
	uint8_t *pucDst;
	uint32_t u4FrameLen;
	uint32_t u4TxdLen;
	uint32_t u4AppendLen;
	uint32_t u4TotalLen;

	/* ------------------------------------------------------------------
	 * Token buffer layout for MT7902 cut-through TX:
	 *
	 *   [ NIC_TX_DESC_AND_PADDING_LENGTH bytes : TXD          ]
	 *   [ u4FrameLen bytes                    : frame payload ]
	 *   [ txd_append_size bytes               : HW TXP append ]
	 *
	 * nicTxFillDesc() has already composed the TXD into
	 * prMsduInfo->aucTxDescBuffer before we are called.
	 * Our job is to assemble these three regions into the pre-allocated
	 * DMA token buffer (prFillToken->prPacket).
	 * ------------------------------------------------------------------
	 */

	if (unlikely(!prGlueInfo || !prGlueInfo->prAdapter ||
		     !prMsduInfo || !prFillToken || !prToken)) {
		DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL arg\\n");
		return;
	}

	prAdapter  = prGlueInfo->prAdapter;
	prChipInfo = prAdapter->chip_info;

	if (unlikely(!prChipInfo)) {
		DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL chip_info\\n");
		return;
	}

	if (unlikely(!prFillToken->prPacket)) {
		DBGLOG(HAL, ERROR,
		       "halTxUpdateCutThroughDesc: fillToken->prPacket NULL (token=%u)\\n",
		       prFillToken->u4Token);
		return;
	}

	prSkb = (struct sk_buff *)prMsduInfo->prPacket;
	if (unlikely(!prSkb || !prSkb->data)) {
		DBGLOG(HAL, ERROR, "halTxUpdateCutThroughDesc: NULL skb\\n");
		return;
	}

	u4FrameLen  = prMsduInfo->u2FrameLength;
	u4TxdLen    = NIC_TX_DESC_AND_PADDING_LENGTH;
	u4AppendLen = prChipInfo->txd_append_size;
	u4TotalLen  = u4TxdLen + u4FrameLen + u4AppendLen;

	if (unlikely(u4FrameLen == 0 || u4FrameLen > 4096)) {
		DBGLOG(HAL, ERROR,
		       "halTxUpdateCutThroughDesc: bad frame len=%u\\n",
		       u4FrameLen);
		return;
	}

	if (unlikely(u4TotalLen > prFillToken->u4DmaLength)) {
		DBGLOG(HAL, ERROR,
		       "halTxUpdateCutThroughDesc: total=%u > dma_buf=%u\\n",
		       u4TotalLen, prFillToken->u4DmaLength);
		return;
	}

	pucDst = (uint8_t *)prFillToken->prPacket;

	/* Region 1: TXD (already composed by nicTxFillDesc into aucTxDescBuffer) */
	kalMemCopy(pucDst, prMsduInfo->aucTxDescBuffer, u4TxdLen);

	/* Region 2: frame payload */
	kalMemCopy(pucDst + u4TxdLen, prSkb->data, u4FrameLen);

	/* Region 3: HW TXP append — zeroed (FW fills in TX status fields) */
	kalMemZero(pucDst + u4TxdLen + u4FrameLen, u4AppendLen);

	/* Update token DMA length to the exact assembled size */
	prFillToken->u4DmaLength = u4TotalLen;

	DBGLOG(HAL, WARN,
	       "[CUTTHRU] token=%u txd=%u frame=%u append=%u total=%u dst=%p\\n",
	       prFillToken->u4Token, u4TxdLen, u4FrameLen, u4AppendLen,
	       u4TotalLen, pucDst);
}"""

with open(path, "r") as f:
    src = f.read()

if OLD not in src:
    raise Exception(f"Old stub not found verbatim in {path} — may already be patched or whitespace differs. Aborting.")

count = src.count(OLD)
if count != 1:
    raise Exception(f"Found {count} occurrences of stub (expected 1). Aborting.")

new_src = src.replace(OLD, NEW, 1)

with open(path, "w") as f:
    f.write(new_src)

print(f"Patched {path} successfully.")
print(f"  Replaced stub ({len(OLD)} chars) with real implementation ({len(NEW)} chars).")
