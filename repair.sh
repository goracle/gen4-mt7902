#!/bin/bash
RLM_DOMAIN="/home/dan/builds/gen4-mt7902/mgmt/rlm_domain.c"

if [ -f "$RLM_DOMAIN" ]; then
    echo "Fixing syntax errors in $RLM_DOMAIN..."
    
    # Remove empty brackets from expressions like variable[]
    # This turns 'pChTxPwrLimit->rTxPwrLimitValue[0][]' into 'pChTxPwrLimit->rTxPwrLimitValue[0]'
    # and '&prCmd->rChannelPowerLimit[]' into '&prCmd->rChannelPowerLimit'
    sed -i 's/\[\]//g' "$RLM_DOMAIN"
    
    echo "Refactor of .c file complete."
else
    echo "Error: rlm_domain.c not found!"
fi
