 /*
 * rak3172_p2p_rui3.cpp
 *
 *  Copyright (C) Daniel Kampert, 2022
 *	Website: www.kampis-elektroecke.de
 *  File info: RAK3172 driver for ESP32.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Errors and commissions should be reported to DanielKampert@kampis-elektroecke.de
 */

#include <sdkconfig.h>

#if((defined CONFIG_RAK3172_USE_RUI3) & (defined CONFIG_RAK3172_WITH_P2P))

#include "rak3172.h"

RAK3172_Error_t RAK3172_P2P_EnableEncryption(RAK3172_t* const p_Device, const uint8_t* const p_Key)
{
    std::string Key;
    char Buffer[3];

    if(p_Key == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+ENCRY=" + std::to_string(true), NULL, NULL));

    p_Device->P2P.isEncryptionEnabled = true;

    // Encode the key into an ASCII string.
    for(uint8_t i = 0x00; i < 8; i++)
    {
        sprintf(Buffer, "%02x", ((uint8_t*)p_Key)[i]);
        Key += std::string(Buffer);
    }

    return RAK3172_SendCommand(p_Device, "AT+ENCKEY=" + Key, NULL, NULL);;
}

RAK3172_Error_t RAK3172_P2P_DisableEncryption(RAK3172_t* const p_Device)
{
    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+ENCRY=" + std::to_string(false), NULL, NULL));

    p_Device->P2P.isEncryptionEnabled = false;

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_P2P_isEncryptionEnabled(const RAK3172_t* const p_Device, bool* p_Enabled)
{
    std::string Value;

    if(p_Enabled == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+ENCRY=?", &Value, NULL));

    *p_Enabled = (bool)std::stoi(Value);

    return RAK3172_ERR_OK;
}

#endif