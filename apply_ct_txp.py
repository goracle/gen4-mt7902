#!/usr/bin/env python3
"""Mgmt-frame TXP for MT7902 (gen4-mt7902).

Run from the repo root:   python3 apply_ct_txp.py [--dry-run]

Layout for mgmt frames sent on the data ring (fgMgmtUseDataQ):
    [TXD 32][TXP 32][frame]       (same total size as before: 94 for a 30B frame)
Ring descriptor SDLen0 = TXD+TXP (64); TXP carries token + payload DMA addr/len.

All-or-nothing: every anchor must match exactly once in every file, otherwise
nothing is written. Whitespace between tokens is matched loosely.
Revert with: git checkout -- include/nic/nic_tx.h os/linux/hif/pcie/pcie.c os/linux/hif/common/hal_pdma.c
"""
import difflib
import pathlib
import re
import sys

args = [a for a in sys.argv[1:] if not a.startswith("-")]
DRY = "--dry-run" in sys.argv
ROOT = pathlib.Path(args[0]) if args else pathlib.Path(".")


def T(s):
    """4-space indents -> tabs (kernel style)."""
    return re.sub(r"(?m)^( {4})+", lambda m: "\t" * (len(m.group(0)) // 4), s)


def loose(anchor):
    parts = re.split(r"\s+", anchor.strip())
    return r"\s+".join(re.escape(p) for p in parts)


def find_once(text, anchor, label):
    ms = list(re.finditer(loose(anchor), text))
    if len(ms) != 1:
        sys.exit("ABORT: anchor '%s' matched %d times (need exactly 1)\n  %s"
                 % (label, len(ms), anchor.strip().splitlines()[0]))
    return ms[0]


def replace(text, anchor, new, label):
    m = find_once(text, anchor, label)
    return text[:m.start()] + new + text[m.end():]


def after(text, anchor, new, label):
    m = find_once(text, anchor, label)
    return text[:m.end()] + new + text[m.end():]


def before(text, anchor, new, label):
    m = find_once(text, anchor, label)
    return text[:m.start()] + new + text[m.start():]


# --------------------------------------------------------------- nic_tx.h
H_MACROS = T(r'''

/*
 * Mgmt frames sent on the data ring use cut-through with the token buffer
 *   [TXD][TXP][frame]
 * NIC_TX_CT_TXP_LEN is the TXP size (== chip_info->txd_append_size, checked
 * at runtime in halTxBuildMgmtCtTxp). The one predicate below is shared by
 * the copy (pcie.c) and the ring/TXP code (hal_pdma.c) so they cannot drift.
 */
#define NIC_TX_CT_TXP_LEN	32
#define NIC_TX_IS_MGMT_CT_TXP(_prMsduInfo) \
    ((_prMsduInfo) && (_prMsduInfo)->eSrc == TX_PACKET_MGMT && \
     (_prMsduInfo)->fgMgmtUseDataQ)
''')


def patch_header(t):
    return after(
        t,
        "#define NIC_TX_DESC_AND_PADDING_LENGTH \\\n"
        "(NIC_TX_DESC_LONG_FORMAT_LENGTH + NIC_TX_DESC_PADDING_LENGTH)",
        H_MACROS, "NIC_TX_DESC_AND_PADDING_LENGTH define")


# ---------------------------------------------------------------- pcie.c
PCIE_COPY = T(r'''/* Layout [TXD][TXP][frame]. The TXP stays zero here: it needs the DMA
     * address, so halFlushToken() builds it after the buffer is mapped. */
    if (u4TxdLen + NIC_TX_CT_TXP_LEN + u4Len > prToken->u4DmaLength) {
        DBGLOG(HAL, ERROR,
               "[TXCOPY] mgmt frame does not fit: txd=%u txp=%u len=%u dma=%u\n",
               u4TxdLen, (uint32_t)NIC_TX_CT_TXP_LEN, u4Len,
               prToken->u4DmaLength);
        return false;
    }
    memcpy(pucDst, prMsduInfo->aucTxDescBuffer, u4TxdLen);
    memset(pucDst + u4TxdLen, 0, NIC_TX_CT_TXP_LEN);
    memcpy(pucDst + u4TxdLen + NIC_TX_CT_TXP_LEN, pucSrc, u4Len);''')
# first line sits where the anchor started (already indented); the rest get
# one more tab to match the original branch body.


def indent_tail(s, tabs):
    lines = s.split("\n")
    return "\n".join([lines[0]] + [("\t" * tabs + l) if l else l for l in lines[1:]])


def patch_pcie(t):
    t = replace(
        t,
        "if (prMsduInfo && prMsduInfo->eSrc == TX_PACKET_MGMT && "
        "prMsduInfo->fgMgmtUseDataQ) {",
        "if (NIC_TX_IS_MGMT_CT_TXP(prMsduInfo)) {", "mgmt predicate")
    t = replace(
        t,
        "memcpy(pucDst, prMsduInfo->aucTxDescBuffer, u4TxdLen); "
        "memcpy(pucDst + u4TxdLen, pucSrc, u4Len);",
        indent_tail(PCIE_COPY, 1), "mgmt copy")
    return t


# ------------------------------------------------------------- hal_pdma.c
HELPER = T(r'''/*
 * Mgmt frames on the data ring use CT with the token buffer laid out as
 *   [TXD 32][TXP 32][frame]
 * The ring descriptor covers TXD+TXP only. The TXP carries the MSDU token
 * and the DMA address/length of the frame, so the firmware fetches the
 * payload itself (the scheme mt76 uses with hw_txp). The payload address is
 * only known once the buffer is mapped, so the TXP is built here: after
 * mapTxBuf() and before flushCache() (which makes it visible to the device).
 */
static bool halTxBuildMgmtCtTxp(struct GLUE_INFO *prGlueInfo,
                struct MSDU_TOKEN_ENTRY *prToken)
{
    struct mt66xx_chip_info *prChipInfo = prGlueInfo->prAdapter->chip_info;
    uint8_t *pucBuf = (uint8_t *)prToken->prPacket;
    uint32_t u4PayloadOfs = NIC_TX_DESC_AND_PADDING_LENGTH + NIC_TX_CT_TXP_LEN;
    static int iDumped;

    /* fillTxDescAppendByHostV2 places the TXP at NIC_TX_DESC_LONG_FORMAT_LENGTH */
    BUILD_BUG_ON(NIC_TX_DESC_AND_PADDING_LENGTH != NIC_TX_DESC_LONG_FORMAT_LENGTH);

    if (!pucBuf || !prToken->rDmaAddr || !prToken->prMsduInfo ||
        !prChipInfo->fillHifAppend ||
        prChipInfo->txd_append_size != NIC_TX_CT_TXP_LEN) {
        DBGLOG(HAL, ERROR,
               "[MGMT-TXP] cannot build: buf=%p dma=0x%llx hook=%p append=%u want=%u\n",
               pucBuf, (unsigned long long)prToken->rDmaAddr,
               prChipInfo->fillHifAppend,
               (uint32_t)prChipInfo->txd_append_size,
               (uint32_t)NIC_TX_CT_TXP_LEN);
        return false;
    }

    kalMemZero(pucBuf + NIC_TX_DESC_AND_PADDING_LENGTH, NIC_TX_CT_TXP_LEN);
    prChipInfo->fillHifAppend(prGlueInfo->prAdapter, prToken->prMsduInfo,
                  prToken->u4Token,
                  prToken->rDmaAddr + u4PayloadOfs,
                  0, TRUE, pucBuf);

    if (iDumped < 8) {
        iDumped++;
        DBGLOG(HAL, WARN,
               "[MGMT-TXP] tok=%u payload dma=0x%llx len=%u\n",
               prToken->u4Token,
               (unsigned long long)(prToken->rDmaAddr + u4PayloadOfs),
               prToken->prMsduInfo->u2FrameLength);
        print_hex_dump(KERN_WARNING, "[MGMT-TXP] TXD+TXP: ",
                   DUMP_PREFIX_OFFSET, 16, 1, pucBuf,
                   u4PayloadOfs, false);
    }
    return true;
}

''')

FLUSH_CALL = T(r'''if (NIC_TX_IS_MGMT_CT_TXP(prToken->prMsduInfo)) {
        if (!halTxBuildMgmtCtTxp(prGlueInfo, prToken))
            return false;
    }

    ''')

SDLEN = T(r'''if (NIC_TX_IS_MGMT_CT_TXP(prToken->prMsduInfo)) {
        /* mgmt CT: descriptor covers [TXD][TXP] only; the TXP points at
         * the frame behind it (see halTxBuildMgmtCtTxp). */
        pTxD->SDLen0 = NIC_TX_DESC_AND_PADDING_LENGTH + NIC_TX_CT_TXP_LEN;
    } else {
        pTxD->SDLen0 = prToken->u4DmaLength;
    }''')


def patch_pdma(t):
    t = replace(t, "pTxD->SDLen0 = prToken->u4DmaLength;", SDLEN, "SDLen0")
    t = before(t, "static bool halFlushToken(struct GLUE_INFO *prGlueInfo,",
               HELPER, "halFlushToken definition")
    t = before(t,
               "if (prMemOps->flushCache) "
               "prMemOps->flushCache(prHifInfo, prToken->prPacket,",
               FLUSH_CALL, "flushCache call")
    return t


FILES = [
    ("include/nic/nic_tx.h", patch_header, "NIC_TX_CT_TXP_LEN"),
    ("os/linux/hif/pcie/pcie.c", patch_pcie, "NIC_TX_IS_MGMT_CT_TXP"),
    ("os/linux/hif/common/hal_pdma.c", patch_pdma, "halTxBuildMgmtCtTxp"),
]

out = []
for rel, fn, marker in FILES:
    p = ROOT / rel
    if not p.is_file():
        sys.exit("ABORT: %s not found (run from the repo root)" % p)
    old = p.read_text()
    if marker in old:
        sys.exit("ABORT: %s already contains %s (already patched?)" % (rel, marker))
    out.append((p, old, fn(old)))

for p, old, new in out:
    diff = difflib.unified_diff(old.splitlines(True), new.splitlines(True),
                                "a/" + str(p), "b/" + str(p))
    if DRY:
        sys.stdout.writelines(diff)
    else:
        p.write_text(new)
        print("patched", p)
print("dry run only, nothing written" if DRY else
      "done. revert with: git checkout -- " + " ".join(r for r, _, _ in FILES))
