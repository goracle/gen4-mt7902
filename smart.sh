#!/bin/bash
# smart-apply-fix.sh - Intelligently apply the auth fix to your actual source

set -e

echo "======================================================================"
echo "MT7902 Auth Frame Flush Fix - Smart Application"
echo "======================================================================"
echo ""

# Verify we're in the right directory
if [[ ! -f "Makefile" ]] || [[ ! -d "mgmt" ]]; then
    echo "❌ Error: Must run from driver source root"
    exit 1
fi

echo "✓ Found driver source tree"
echo ""

# Create backup
BACKUP_DIR="backup-auth-fix-$(date +%Y%m%d-%H%M%S)"
echo "Creating backup in $BACKUP_DIR ..."
mkdir -p "$BACKUP_DIR"
cp -r include/mgmt/ais_fsm.h mgmt/ais_fsm.c os/linux/gl_cfg80211.c "$BACKUP_DIR/" 2>/dev/null || true
echo "✓ Backup created"
echo ""

# ============================================================================
# CHANGE 1: Add flag to struct AIS_FSM_INFO
# ============================================================================
echo "1. Adding fgIsCfg80211Connecting flag to struct AIS_FSM_INFO..."

# Find the struct definition and add the flag after u4ScanReportStartTime
awk '
/struct AIS_FSM_INFO {/ { in_struct = 1 }
in_struct && /uint32_t u4ScanReportStartTime;/ {
    print
    print "\t/* Flag to prevent AIS auto-reconnect when cfg80211/userspace is driving connection */"
    print "\tu_int8_t fgIsCfg80211Connecting;"
    print ""
    next
}
{ print }
' include/mgmt/ais_fsm.h > include/mgmt/ais_fsm.h.tmp && mv include/mgmt/ais_fsm.h.tmp include/mgmt/ais_fsm.h

echo "  ✓ Flag added to include/mgmt/ais_fsm.h"
echo ""

# ============================================================================
# CHANGE 2: Guard auto-reconnect in aisFsmStateSearchAction - PHASE 0
# ============================================================================
echo "2. Guarding auto-reconnect in aisFsmStateSearchAction (phase 0)..."

# Find phase 0 and add guard before aisFsmInsertRequest(AIS_REQUEST_RECONNECT)
awk '
/AIS_FSM_STATE_SEARCH_ACTION_PHASE_0/ { phase0 = 1 }
phase0 && /if \(prConnSettings->eOPMode == NET_TYPE_INFRA\)/ { found_infra = 1 }
found_infra && /aisFsmInsertRequest\(prAdapter, AIS_REQUEST_RECONNECT,/ {
    print "\t\t\t/* Check if cfg80211/userspace is driving this connection */"
    print "\t\t\tif (prAisFsmInfo->fgIsCfg80211Connecting) {"
    print "\t\t\t\tDBGLOG(AIS, INFO,"
    print "\t\t\t\t\t\"[AIS%d] cfg80211 owns connection (skip driver auto-reconnect phase 0)\\n\","
    print "\t\t\t\t\tucBssIndex);"
    print "\t\t\t\teState = AIS_STATE_IDLE;"
    print "\t\t\t} else {"
    print "\t\t\t\t" $0
    # Next line is ucBssIndex); - need to close the else
    getline
    print "\t\t\t\t" $0
    print "\t\t\t}"
    found_infra = 0
    phase0 = 0
    next
}
{ print }
' mgmt/ais_fsm.c > mgmt/ais_fsm.c.tmp && mv mgmt/ais_fsm.c.tmp mgmt/ais_fsm.c

echo "  ✓ Phase 0 guard added"
echo ""

# ============================================================================
# CHANGE 3: Guard auto-reconnect in aisFsmStateSearchAction - PHASE 1
# ============================================================================
echo "3. Guarding auto-reconnect in aisFsmStateSearchAction (phase 1)..."

awk '
/AIS_FSM_STATE_SEARCH_ACTION_PHASE_1/ { phase1 = 1 }
phase1 && /if \(prConnSettings->eOPMode == NET_TYPE_INFRA\)/ && !done_phase1 {
    print
    # Skip the blank lines and comments
    getline
    while ($0 ~ /^[ \t]*$/ || $0 ~ /\/\*/) {
        print
        getline
    }
    # Now we should be at aisFsmInsertRequest
    if ($0 ~ /aisFsmInsertRequest\(prAdapter,/) {
        print ""
        print "\t\t\t\t/* Check if cfg80211/userspace is driving this connection */"
        print "\t\t\t\tif (prAisFsmInfo->fgIsCfg80211Connecting) {"
        print "\t\t\t\t\tDBGLOG(AIS, INFO,"
        print "\t\t\t\t\t\t\"[AIS%d] cfg80211 owns connection (skip driver auto-reconnect phase 1)\\n\","
        print "\t\t\t\t\t\tucBssIndex);"
        print "\t\t\t\t\teState = AIS_STATE_IDLE;"
        print "\t\t\t\t} else {"
        print "\t\t\t\t\t" $0
        getline  # AIS_REQUEST_RECONNECT,
        print "\t\t\t\t\t" $0
        getline  # ucBssIndex);
        print "\t\t\t\t\t" $0
        print "\t\t\t\t}"
        done_phase1 = 1
        next
    }
}
{ print }
' mgmt/ais_fsm.c > mgmt/ais_fsm.c.tmp && mv mgmt/ais_fsm.c.tmp mgmt/ais_fsm.c

echo "  ✓ Phase 1 guard added"
echo ""

# ============================================================================
# CHANGE 4: Set flag in mtk_cfg80211_auth
# ============================================================================
echo "4. Setting flag in mtk_cfg80211_auth..."

# Add before the final return 0
awk '
/^int mtk_cfg80211_auth/ { in_auth = 1 }
in_auth && /return 0;/ && !done_auth {
    print "\t/* Mark that cfg80211/userspace is driving this connection */"
    print "\t{"
    print "\t\tstruct AIS_FSM_INFO *prAisFsmInfo = aisGetAisFsmInfo(prGlueInfo->prAdapter, ucBssIndex);"
    print "\t\tif (prAisFsmInfo) {"
    print "\t\t\tprAisFsmInfo->fgIsCfg80211Connecting = TRUE;"
    print "\t\t\tDBGLOG(REQ, INFO, \"[CFG80211] Marked cfg80211-connecting for BSS %u\\n\", ucBssIndex);"
    print "\t\t}"
    print "\t}"
    print ""
    done_auth = 1
}
{ print }
in_auth && /^}/ { in_auth = 0 }
' os/linux/gl_cfg80211.c > os/linux/gl_cfg80211.c.tmp && mv os/linux/gl_cfg80211.c.tmp os/linux/gl_cfg80211.c

echo "  ✓ Flag set in mtk_cfg80211_auth"
echo ""

# ============================================================================
# CHANGE 5: Clear flag on join success
# ============================================================================
echo "5. Clearing flag on join completion..."

# Find the success path in aisFsmJoinCompleteAction
awk '
/enum ENUM_AIS_STATE aisFsmJoinCompleteAction/ { in_join_complete = 1 }
in_join_complete && /prAisFsmInfo->prTargetBssDesc->fgDeauthLastTime = FALSE;/ {
    print "\t\t/* Clear cfg80211 connecting flag on successful join */"
    print "\t\tprAisFsmInfo->fgIsCfg80211Connecting = FALSE;"
    print ""
}
{ print }
' mgmt/ais_fsm.c > mgmt/ais_fsm.c.tmp && mv mgmt/ais_fsm.c.tmp mgmt/ais_fsm.c

echo "  ✓ Flag cleared on success path"
echo ""

# ============================================================================
# CHANGE 6: Clear flag on join failure paths
# ============================================================================
echo "6. Adding failure path flag clears..."

# Add clears before key failure transitions
awk '
/eNextState = AIS_STATE_JOIN_FAILURE/ {
    # Add clear before setting join failure state
    indent = match($0, /[^ \t]/)
    spaces = substr($0, 1, indent-1)
    print spaces "/* Clear cfg80211 connecting flag on join failure */"
    print spaces "prAisFsmInfo->fgIsCfg80211Connecting = FALSE;"
    print ""
}
{ print }
' mgmt/ais_fsm.c > mgmt/ais_fsm.c.tmp && mv mgmt/ais_fsm.c.tmp mgmt/ais_fsm.c

echo "  ✓ Flag clears added to failure paths"
echo ""

echo "======================================================================"
echo "✓ All changes applied successfully!"
echo "======================================================================"
echo ""
echo "Next steps:"
echo "  1. Review changes:"
echo "       git diff include/mgmt/ais_fsm.h"
echo "       git diff mgmt/ais_fsm.c"
echo "       git diff os/linux/gl_cfg80211.c"
echo ""
echo "  2. Rebuild:"
echo "       make clean && make -j\$(nproc)"
echo ""
echo "  3. Test:"
echo "       sudo rmmod mt7902"
echo "       sudo ./load.sh"
echo "       iwctl station wlan0 connect H"
echo ""
echo "  4. Watch logs:"
echo "       sudo dmesg -w | grep -E 'CFG80211|cfg80211|TX_DONE|FLUSHED'"
echo ""
echo "Backup saved in: $BACKUP_DIR"
echo "======================================================================"
