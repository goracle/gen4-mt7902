#!/bin/bash
# MT7902 Bug Hunt - Grep Command Reference
# Pipe any of these to 'copy' for quick clipboard access

echo "=== CHANNEL INDEXING SUSPECTS ==="
echo "grep -rn 'ucChannelNum.*\[' mgmt/ nic/"
echo "grep -rn 'u2PriChnlFreq.*\[' mgmt/ nic/"
echo "grep -rn 'channel.*-.*[0-9][0-9].*\[' mgmt/"
echo ""

echo "=== COUNTRY CODE / REGULATORY ==="
echo "grep -rn 'u2CountryCode.*00' mgmt/"
echo "grep -rn 'world.*domain\|WORLD.*DOMAIN' mgmt/ include/"
echo "grep -rn 'rlmDomainGetCountry\|rlmDomainOidSetCountry' mgmt/"
echo ""

echo "=== SCAN STATE MACHINE ==="
echo "grep -rn 'scnEventScanDone\|scnFsmSteps' mgmt/scan*.c"
echo "grep -rn 'SCAN_STATE.*=' mgmt/scan_fsm.c"
echo "grep -rn 'prScanInfo->eCurrentState' mgmt/"
echo ""

echo "=== BANDWIDTH ARRAYS ==="
echo "grep -rn 'rlmDomainGetChannelBw' mgmt/"
echo "grep -rn 'cnmGetBssMaxBw' mgmt/"
echo "grep -rn '\(bw\|BW\|bandwidth\).*\[.*\].*=' mgmt/ nic/ | grep -v '//' | head -20"
echo ""

echo "=== ABSENCE BLOCKING ==="
echo "grep -rn 'absence.*block\|block.*absence' mgmt/ nic/"
echo "grep -rn 'cnmTimerStopTimer\|cnmTimerStartTimer' mgmt/cnm*.c"
echo ""

echo "=== ARRAY BOUNDS CHECKS (or lack thereof) ==="
echo "grep -rn 'if.*<.*ARRAY_SIZE\|WARN_ON.*>=' mgmt/ nic/ | head -10"
echo "grep -rn 'BUG_ON\|WARN_ON' mgmt/scan*.c mgmt/rlm*.c"
echo ""

echo "=== KEY FUNCTIONS TO REVIEW ==="
cat << 'EOF'
# Channel to Index conversions
grep -A 10 "rlmDomainGetChannelBw" mgmt/rlm_domain.c
grep -A 10 "cnmGetBssMaxBw" mgmt/cnm.c

# Scan completion
grep -A 30 "scnEventScanDone" mgmt/scan_fsm.c

# Country code updates
grep -A 20 "rlmDomainOidSetCountry" mgmt/rlm_domain.c

# Channel list rebuilds
grep -B 5 -A 15 "buildChannelList\|BuildChannelList" mgmt/
EOF
