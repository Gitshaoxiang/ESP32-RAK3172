 /*
 * rak3172_lorawan.cpp
 *
 *  Copyright (C) Daniel Kampert, 2022
 *	Website: www.kampis-elektroecke.de
 *  File info: LoRaWAN module of the RAK3172 driver for ESP32.

  GNU GENERAL PUBLIC LICENSE:
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.

  Errors and commissions should be reported to DanielKampert@kampis-elektroecke.de
 */

#include <sdkconfig.h>

#ifdef CONFIG_RAK3172_WITH_LORAWAN

#include <esp_log.h>
#include <esp_sleep.h>

#include "rak3172.h"

static const char* TAG = "RAK3172_LoRaWAN";

RAK3172_Error_t RAK3172_LoRaWAN_Init(RAK3172_t* const p_Device, uint8_t TxPwr, uint8_t Retries, RAK3172_JoinMode_t JoinMode, const uint8_t* const p_Key1, const uint8_t* const p_Key2, const uint8_t* const p_Key3, char Class, RAK3172_Band_t Band, RAK3172_SubBand_t Subband, bool UseADR, uint32_t Timeout)
{
    std::string Command;

    if((p_Device == NULL) || ((Class != 'A') && (Class != 'B') && (Class != 'C')) || (p_Key1 == NULL) || (p_Key2 == NULL) || (p_Key3 == NULL))
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initialize module in LoRaWAN mode...");
    RAK3172_ERROR_CHECK(RAK3172_SetMode(p_Device, RAK_MODE_LORAWAN));

    // Stop an ongoing joining process.
    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_StopJoin(p_Device));
    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_isJoined(p_Device, &p_Device->LoRaWAN.isJoined));

    p_Device->Internal.isBusy = false;

    Command = "AT+CLASS=";
    Command += Class;
    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, Command, NULL, NULL));

	RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetADR(p_Device, UseADR));
    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetBand(p_Device, Band));

    if(Subband != RAK_SUB_BAND_NONE)
    {
        RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetSubBand(p_Device, Subband))
    }

    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetRetries(p_Device, Retries));
    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetTxPwr(p_Device, TxPwr));
    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetJoinMode(p_Device, JoinMode));

    p_Device->LoRaWAN.Join = JoinMode;
    if(p_Device->LoRaWAN.Join == RAK_JOIN_OTAA)
    {
        ESP_LOGI(TAG, "Using OTAA mode");

        return RAK3172_LoRaWAN_SetOTAAKeys(p_Device, p_Key1, p_Key2, p_Key3);
    }
    else
    {
        ESP_LOGI(TAG, "Using ABP mode");

        return RAK3172_LoRaWAN_SetABPKeys(p_Device, p_Key1, p_Key2, p_Key3);
    }

    return RAK3172_ERR_INVALID_ARG;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetOTAAKeys(const RAK3172_t* const p_Device, const uint8_t* const p_DEVEUI, const uint8_t* const p_APPEUI, const uint8_t* const p_APPKEY)
{
    std::string AppEUIString;
    std::string DevEUIString;
    std::string AppKeyString;

    if((p_Device == NULL) || (p_DEVEUI == NULL) || (p_APPEUI == NULL) || (p_APPKEY == NULL))
    {
        return RAK3172_ERR_INVALID_ARG;
    }
    else if(p_Device->LoRaWAN.Join != RAK_JOIN_OTAA)
    {
        return RAK3172_ERR_INVALID_STATE;
    }

    // Copy the keys from the buffer into a string.
    char Buffer[3];
    for(uint8_t i = 0; i < 8; i++)
    {
        sprintf(Buffer, "%02X", p_APPEUI[i]);
        AppEUIString += std::string(Buffer);

        sprintf(Buffer, "%02X", p_DEVEUI[i]);
        DevEUIString += std::string(Buffer);
    }

    for(uint8_t i = 0; i < 16; i++)
    {
        sprintf(Buffer, "%02X", p_APPKEY[i]);
        AppKeyString += std::string(Buffer);
    }

    ESP_LOGD(TAG, "DEVEUI: %s - Size: %u", DevEUIString.c_str(), DevEUIString.length());
    ESP_LOGD(TAG, "APPEUI: %s - Size: %u", AppEUIString.c_str(), AppEUIString.length());
    ESP_LOGD(TAG, "APPKEY: %s - Size: %u", AppKeyString.c_str(), AppKeyString.length());

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+DEVEUI=" + DevEUIString, NULL, NULL));
    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+APPEUI=" + AppEUIString, NULL, NULL));
    return RAK3172_SendCommand(p_Device, "AT+APPKEY=" + AppKeyString, NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_SetABPKeys(const RAK3172_t* const p_Device, const uint8_t* const p_APPSKEY, const uint8_t* const p_NWKSKEY, const uint8_t* const p_DEVADDR)
{
    std::string AppSKEYString;
    std::string NwkSKEYString;
    std::string DevADDRString;

    if((p_Device == NULL) || (p_APPSKEY == NULL) || (p_NWKSKEY == NULL) || (p_DEVADDR == NULL))
    {
        return RAK3172_ERR_INVALID_ARG;
    }
    else if(p_Device->LoRaWAN.Join != RAK_JOIN_ABP)
    {
        return RAK3172_ERR_INVALID_STATE;
    }

    // Copy the keys from the buffer into a string.
    char Buffer[3];
    for(uint8_t i = 0; i < 16; i++)
    {
        sprintf(Buffer, "%02X", p_APPSKEY[i]);
        AppSKEYString += std::string(Buffer);

        sprintf(Buffer, "%02X", p_NWKSKEY[i]);
        NwkSKEYString += std::string(Buffer);
    }

    for(uint8_t i = 0; i < 4; i++)
    {
        sprintf(Buffer, "%02X", p_DEVADDR[i]);
        DevADDRString += std::string(Buffer);
    }

    ESP_LOGD(TAG, "APPSKEY: %s - Size: %u", AppSKEYString.c_str(), AppSKEYString.length());
    ESP_LOGD(TAG, "NWKSKEY: %s - Size: %u", NwkSKEYString.c_str(), NwkSKEYString.length());
    ESP_LOGD(TAG, "DEVADDR: %s - Size: %u", DevADDRString.c_str(), DevADDRString.length());

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+APPSKEY=" + AppSKEYString, NULL, NULL));
    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+NWKSKEY=" + NwkSKEYString, NULL, NULL));
    return RAK3172_SendCommand(p_Device, "AT+DEVADDR=" + DevADDRString, NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_StartJoin(const RAK3172_t* const p_Device, uint32_t Timeout, uint8_t Attempts, bool EnableAutoJoin, uint8_t Interval, RAK3172_Wait_t on_Wait)
{
    uint32_t TimeNow;

    if((p_Device == NULL) || (Attempts == 0))
    {
        return RAK3172_ERR_INVALID_ARG;
    }
    else if(p_Device->LoRaWAN.isJoined)
    {
        return RAK3172_ERR_OK;
    }

    // Start the joining procedure.
    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+JOIN=1:" + std::to_string(EnableAutoJoin) + ":" + std::to_string(Interval) + ":" + std::to_string(Attempts), NULL, NULL));

    // Wait for a successfull join.
    TimeNow = esp_timer_get_time() / 1000ULL;
    do
    {
        if((Timeout > 0) && (((esp_timer_get_time() / 1000ULL) - TimeNow) >= (Timeout * 1000ULL)))
        {
            ESP_LOGE(TAG, "Join timeout!");

            RAK3172_LoRaWAN_StopJoin(p_Device);

            return RAK3172_ERR_TIMEOUT;
        }

        esp_sleep_enable_timer_wakeup(20 * 1000ULL);
        esp_light_sleep_start();
        vTaskDelay(20);
    } while(p_Device->LoRaWAN.isJoined == false);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_StopJoin(const RAK3172_t* const p_Device)
{
    return RAK3172_SendCommand(p_Device, "AT+JOIN=0:0:7:0", NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_isJoined(RAK3172_t* const p_Device, bool* const p_Joined)
{
    std::string Value;

    if((p_Device == NULL) || (p_Joined == NULL))
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    p_Device->LoRaWAN.isJoined = false;

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+NJS=?", &Value, NULL));

    if(Value.compare("1") == 0)
    {
        p_Device->LoRaWAN.isJoined = true;
    }

    *p_Joined = p_Device->LoRaWAN.isJoined;

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_Transmit(RAK3172_t* const p_Device, uint8_t Port, const uint8_t* const p_Buffer, uint8_t Length)
{
    return RAK3172_LoRaWAN_Transmit(p_Device, Port, p_Buffer, Length, 0);
}

RAK3172_Error_t RAK3172_LoRaWAN_Transmit(RAK3172_t* const p_Device, uint8_t Port, const void* p_Buffer, uint8_t Length, uint32_t Timeout, bool Confirmed, RAK3172_Wait_t Wait)
{
    std::string Payload;
    std::string Status;
    uint32_t Now;
    char Buffer[3];

    if((p_Device == NULL) || ((p_Buffer == NULL) && (Length == 0)) || (Port == 0))
    {
        return RAK3172_ERR_INVALID_ARG;
    }
    else if(p_Device->LoRaWAN.isJoined == false)
    {
        return RAK3172_ERR_NOT_CONNECTED;
    }
    else if(Length == 0)
    {
        return RAK3172_ERR_OK;
    }

    // Encode the payload into an ASCII string.
    for(uint8_t i = 0x00; i < Length; i++)
    {
        sprintf(Buffer, "%02x", ((uint8_t*)p_Buffer)[i]);
        Payload += std::string(Buffer);
    }

    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_SetConfirmation(p_Device, Confirmed));

    // TODO: Use long data send when the packet size is greater than 242 bytes.
    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+SEND=" + std::to_string(Port) + ":" + Payload, NULL, &Status));

    // The device is busy. Leave the function with an invalid state error.
    if(Status.find("AT_BUSY_ERROR") != std::string::npos)
    {
        return RAK3172_ERR_INVALID_RESPONSE;
    }

    // Wait for the confirmation if needed.
    if(Confirmed)
    {
        Now = esp_timer_get_time() / 1000ULL;
        do
        {
            std::string* Line;

            if(Wait)
            {
                Wait();
            }

            if(xQueueReceive(p_Device->Internal.RxQueue, &Line, RAK3172_WAIT_TIMEOUT / portTICK_PERIOD_MS) == pdPASS)
            {
                ESP_LOGI(TAG, "Transmission event: %s", Line->c_str());

                // Transmission failed.
                if(Line->find("SEND CONFIRMED FAILED") != std::string::npos)
                {
                    return RAK3172_ERR_INVALID_RESPONSE;
                }
                // Transmission was successful.
                else if(Line->find("SEND CONFIRMED OK") != std::string::npos)
                {
                    return RAK3172_ERR_OK;
                }

                delete Line;
            }
            else
            {
                ESP_LOGD(TAG, "Wait for Tx...");
            }

            if((Timeout > 0) && (((esp_timer_get_time() / 1000ULL) - Now) >= (Timeout * 1000UL)))
            {
                ESP_LOGE(TAG, "Transmit timeout!");

                return RAK3172_ERR_TIMEOUT;
            }

            esp_sleep_enable_timer_wakeup(20 * 1000ULL);
            esp_light_sleep_start();
            vTaskDelay(20);
        } while(true);
    }

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_Receive(RAK3172_t* const p_Device, std::string* const p_Payload, int* p_RSSI, int* p_SNR, uint32_t Timeout)
{
    uint32_t Now;

    if((p_Device == NULL) || (p_Payload == NULL) || (Timeout <= 1))
    {
        return RAK3172_ERR_INVALID_ARG;
    }
    else if(p_Device->LoRaWAN.isJoined == false)
    {
        return RAK3172_ERR_NOT_CONNECTED;
    }

    Now = esp_timer_get_time() / 1000ULL;
    do
    {
        std::string* Line;

        if(xQueueReceive(p_Device->Internal.RxQueue, &Line, RAK3172_WAIT_TIMEOUT / portTICK_PERIOD_MS) == pdPASS)
        {
            int Index;

            ESP_LOGD(TAG, "Receive event: %s", Line->c_str());

            // Get the RX metadata first.
            if(Line->find("RX") != std::string::npos)
            {
                std::string Dummy;

                // Get the RSSI value.
                if(p_RSSI != NULL)
                {
                    Index = Line->find(",");
                    Dummy = Line->substr(Index + 7, Index + Line->find(",", Index) + 1);
                    *p_RSSI = std::stoi(Dummy);
                }

                // Get the SNR value.
                if(p_SNR != NULL)
                {
                    Index = Line->find_last_of(",");
                    Dummy = Line->substr(Index + 6, Line->length() - 1);
                    *p_SNR = std::stoi(Dummy);
                }
            }

            // Then get the data and leave the function.
            if(Line->find("UNICAST") != std::string::npos)
            {
                //Line = p_Device->p_Interface->readStringUntil('\n');
                ESP_LOGD(TAG, "    Payload: %s", Line->c_str());

                // Clean up the payload string ("+EVT:Port:Payload")
                //  - Remove the "+EVT" indicator
                //  - Remove the port number
                *p_Payload = Line->substr(Line->find_last_of(":") + 1, Line->length());

                return RAK3172_ERR_OK;
            }
        }

        delete Line;

        if((Timeout > 0) && ((((esp_timer_get_time() / 1000ULL) - Now) / 1000ULL) >= Timeout))
        {
            ESP_LOGE(TAG, "Receive timeout!");

            return RAK3172_ERR_TIMEOUT;
        }

        esp_sleep_enable_timer_wakeup(20 * 1000ULL);
        esp_light_sleep_start();
        vTaskDelay(20);
    } while(true);
}

RAK3172_Error_t RAK3172_LoRaWAN_SetRetries(const RAK3172_t* const p_Device, uint8_t Retries)
{
    bool Enable = false;

    if(Retries > 7)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    if(Retries > 0)
    {
        Enable = true;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+CFM=" + std::to_string(Enable), NULL, NULL));

    return RAK3172_SendCommand(p_Device, "AT+RETY=" + std::to_string(Retries), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetRetries(const RAK3172_t* const p_Device, uint8_t* const p_Retries)
{
    std::string Value;

    if(p_Retries == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+RETY=?", &Value, NULL));

    *p_Retries = (uint8_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetPNM(const RAK3172_t* const p_Device, bool Enable)
{
    return RAK3172_SendCommand(p_Device, "AT+PNM=" + std::to_string(Enable), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetPNM(const RAK3172_t* const p_Device, bool* const p_Enable)
{
    std::string Value;

    if(p_Enable == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+PNM=?", &Value, NULL));

    *p_Enable = (bool)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetConfirmation(const RAK3172_t* const p_Device, bool Enable)
{
    return RAK3172_SendCommand(p_Device, "AT+CFM=" + std::to_string(Enable), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetConfirmation(const RAK3172_t* const p_Device, bool* const p_Enable)
{
    std::string Value;

    if(p_Enable == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+CFM=?", &Value, NULL));

    *p_Enable = (bool)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetBand(const RAK3172_t* const p_Device, RAK3172_Band_t Band)
{
    return RAK3172_SendCommand(p_Device, "AT+BAND=" + std::to_string((uint8_t)Band), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetBand(const RAK3172_t* const p_Device, RAK3172_Band_t* p_Band)
{
    std::string Value;

    if(p_Band == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+BAND=?", &Value, NULL));

    *p_Band = (RAK3172_Band_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetSubBand(const RAK3172_t* const p_Device, RAK3172_SubBand_t Band)
{
    RAK3172_Band_t Dummy;

    if(Band == RAK_SUB_BAND_NONE)
    {
        return RAK3172_ERR_OK;
    }

    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_GetBand(p_Device, &Dummy));

    // The sub band can only be changed when using US915, AU915 or CN470 frequency band.
    if((Dummy == RAK_BAND_US915) || (Dummy == RAK_BAND_AU915) || (Dummy == RAK_BAND_CN470))
    {
        if((Band > RAK_SUB_BAND_9) && (Dummy != RAK_BAND_CN470))
        {
            return RAK3172_ERR_INVALID_ARG;
        }

        if(Band == RAK_SUB_BAND_ALL)
        {
            return RAK3172_SendCommand(p_Device, "AT+MASK=0000", NULL, NULL);
        }
        else
        {
            char Temp[5];
            uint32_t Mask = 1;

            Mask = Mask << (Band - 2);

            sprintf(Temp, "%04X", Mask);
    
            return RAK3172_SendCommand(p_Device, "AT+MASK=" + std::string(Temp), NULL, NULL);
        }
    }

    return RAK3172_ERR_FAIL;
}

RAK3172_Error_t RAK3172_LoRaWAN_GetSubBand(const RAK3172_t* const p_Device, RAK3172_SubBand_t* p_Band)
{
    RAK3172_Band_t Dummy;
    std::string Value;
    uint32_t Mask;
    uint8_t Shifts = 1;

    if(p_Band == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_GetBand(p_Device, &Dummy));

    if((Dummy != RAK_BAND_US915) && (Dummy != RAK_BAND_AU915) && (Dummy != RAK_BAND_CN470))
    {
        *p_Band = RAK_SUB_BAND_NONE;

        return RAK3172_ERR_OK;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+MASK=?", &Value, NULL));

    Mask = std::stoi(Value);

    if(Mask == 0)
    {
        *p_Band = RAK_SUB_BAND_ALL;

        return RAK3172_ERR_OK;
    }
    else
    {
        while(Mask != 1)
        {
            Mask >>= 1;
            Shifts++;
        }

        *p_Band = (RAK3172_SubBand_t)(Shifts + 2);

        return RAK3172_ERR_OK;
    }
}

RAK3172_Error_t RAK3172_LoRaWAN_SetTxPwr(const RAK3172_t* const p_Device, uint8_t TxPwr)
{
    uint8_t TxPwrIndex = 0;
    RAK3172_Band_t Band;

    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_GetBand(p_Device, &Band));

    ESP_LOGD(TAG, "Set Tx power to: %u dBm", TxPwr);

    // For EU868 the maximum transmit power is +16 dB EIRP.
    if(Band == RAK_BAND_EU868)
    {
        const uint8_t EIRP = 16;

        if(TxPwr >= EIRP)
        {
            TxPwrIndex = 0;
        }
        else if(TxPwr < (EIRP - 14))
        {
            TxPwrIndex = 10;
        }
        else
        {
            TxPwrIndex = (uint8_t)((EIRP - TxPwr) / 2);
        }
    }
    // For US915 the maximum transmit power is +30 dBm conducted power.
    else if(Band == RAK_BAND_US915)
    {
        const uint8_t MaxPwr = 30;

        if(TxPwr >= MaxPwr)
        {
            TxPwrIndex = 0;
        }
        else if(TxPwr < 10)
        {
            TxPwrIndex = 10;
        }
        else
        {
            TxPwrIndex = (uint8_t)((MaxPwr - TxPwr) / 2);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Tx power is not implemented for the selected frequency band!");
    }

    ESP_LOGD(TAG, "Set Tx power index: %u", TxPwrIndex);

    return RAK3172_SendCommand(p_Device, "AT+TXP=" + std::to_string(TxPwrIndex), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_SetRX1Delay(const RAK3172_t* const p_Device, uint16_t Delay)
{
    return RAK3172_SendCommand(p_Device, "AT+RX1DL=" + std::to_string(Delay), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_SetRX2Delay(const RAK3172_t* const p_Device, uint16_t Delay)
{
    return RAK3172_SendCommand(p_Device, "AT+RX2DL=" + std::to_string(Delay), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetSNR(const RAK3172_t* const p_Device, uint8_t* const p_SNR)
{
    std::string Value;

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+SNR=?", &Value, NULL));

    *p_SNR = (uint8_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_GetRSSI(const RAK3172_t* const p_Device, int8_t* p_RSSI)
{
    std::string Value;

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+RSSI=?", &Value, NULL));

    *p_RSSI = (int8_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_GetDuty(const RAK3172_t* const p_Device, uint8_t* const p_Duty)
{
    std::string Value;
    RAK3172_Band_t Band;

    RAK3172_ERROR_CHECK(RAK3172_LoRaWAN_GetBand(p_Device, &Band));

    if((Band != RAK_BAND_EU868) && (Band != RAK_BAND_RU864) && (Band != RAK_BAND_EU433))
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+DUTYTIME=?", &Value, NULL));

    *p_Duty = (uint8_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetDataRate(const RAK3172_t* const p_Device, RAK3172_DataRate_t DR)
{
    if(DR > RAK_DR_7)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    return RAK3172_SendCommand(p_Device, "AT+DR=" + std::to_string(DR), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetDataRate(const RAK3172_t* const p_Device, RAK3172_DataRate_t* p_DR)
{
    std::string Value;

    if(p_DR == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+DR=?", &Value, NULL));

    *p_DR = (RAK3172_DataRate_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetADR(const RAK3172_t* const p_Device, bool Enable)
{
    return RAK3172_SendCommand(p_Device, "AT+ADR=" + std::to_string(Enable), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetADR(const RAK3172_t* const p_Device, bool* const p_Enable)
{
    std::string Value;

    if(p_Enable == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+ADR=?", &Value, NULL));

    *p_Enable = (bool)std::stoi(Value);

    return RAK3172_ERR_OK;
}

RAK3172_Error_t RAK3172_LoRaWAN_SetJoinMode(const RAK3172_t* const p_Device, RAK3172_JoinMode_t Mode)
{
    return RAK3172_SendCommand(p_Device, "AT+NJM=" + std::to_string(Mode), NULL, NULL);
}

RAK3172_Error_t RAK3172_LoRaWAN_GetJoinMode(const RAK3172_t* const p_Device, RAK3172_JoinMode_t* p_Mode)
{
    std::string Value;

    if(p_Mode == NULL)
    {
        return RAK3172_ERR_INVALID_ARG;
    }

    RAK3172_ERROR_CHECK(RAK3172_SendCommand(p_Device, "AT+NJM=?", &Value, NULL));

    *p_Mode = (RAK3172_JoinMode_t)std::stoi(Value);

    return RAK3172_ERR_OK;
}

#endif