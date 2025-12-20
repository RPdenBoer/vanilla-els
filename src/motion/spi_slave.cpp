#include "spi_slave.h"
#include "config_motion.h"
#include <Arduino.h>
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"

// Static member initialization
CommandPacket SpiSlave::last_command = {};
StatusPacket SpiSlave::current_status = {};
volatile bool SpiSlave::transaction_pending = false;
bool SpiSlave::connected = false;
uint32_t SpiSlave::last_rx_ms = 0;

// DMA-capable buffers - must be in DMA-capable memory and 32-bit aligned
// Using 64 bytes to ensure proper alignment and avoid DMA edge effects
WORD_ALIGNED_ATTR DMA_ATTR static uint8_t rx_buffer[64];
WORD_ALIGNED_ATTR DMA_ATTR static uint8_t tx_buffer[64];

// Transaction descriptor
static spi_slave_transaction_t slave_trans;

// Callback after transaction completes
static void IRAM_ATTR spi_post_trans_cb(spi_slave_transaction_t *trans) {
    SpiSlave::transaction_pending = true;
}

bool SpiSlave::init() {
    // Zero the buffers completely
    memset(rx_buffer, 0, sizeof(rx_buffer));
    memset(tx_buffer, 0, sizeof(tx_buffer));
    
    // Configure SPI bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = SPI_SLAVE_MOSI;
    bus_cfg.miso_io_num = SPI_SLAVE_MISO;
    bus_cfg.sclk_io_num = SPI_SLAVE_CLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 64;
    
    // Configure slave interface
    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.spics_io_num = SPI_SLAVE_CS;
    slave_cfg.flags = 0;
    slave_cfg.queue_size = 1;
    slave_cfg.mode = 0;  // SPI Mode 0
    slave_cfg.post_trans_cb = spi_post_trans_cb;
    
    // Initialize SPI slave - try without DMA first to rule out DMA issues
    esp_err_t err = spi_slave_initialize(SPI2_HOST, &bus_cfg, &slave_cfg, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        Serial.printf("[SPI] Slave init failed: %d\n", err);
        return false;
    }
    
    // Prepare initial status with valid checksum
    memset(&current_status, 0, sizeof(current_status));
    current_status.version = PROTOCOL_VERSION;
    
    // Queue first transaction
    memset(&slave_trans, 0, sizeof(slave_trans));
    slave_trans.length = PROTOCOL_PACKET_SIZE * 8;  // Length in bits
    slave_trans.rx_buffer = rx_buffer;
    slave_trans.tx_buffer = tx_buffer;
    
    // Copy initial status to TX buffer with valid checksum
    memcpy(tx_buffer, &current_status, sizeof(current_status));
    protocolSign(tx_buffer, sizeof(current_status));
    
    // Debug: verify initial packet is valid
    Serial.printf("[SPI] Initial TX: ver=%d, chk=0x%02X\n", 
        tx_buffer[0], tx_buffer[PROTOCOL_PACKET_SIZE-1]);
    
    spi_slave_queue_trans(SPI2_HOST, &slave_trans, portMAX_DELAY);
    
    Serial.println("[SPI] Slave initialized");
    return true;
}

void SpiSlave::process() {
    if (!transaction_pending) {
        // Check for timeout
        if (connected && (millis() - last_rx_ms > 1000)) {
            connected = false;
            Serial.println("[SPI] Connection lost");
        }
        return;
    }
    
    transaction_pending = false;
    last_rx_ms = millis();
    
    // Validate received command
    bool valid = protocolValidate(rx_buffer, PROTOCOL_PACKET_SIZE);
    if (valid) {
        CommandPacket* cmd = (CommandPacket*)rx_buffer;
        
        if (cmd->version == PROTOCOL_VERSION) {
            // Copy to last_command for other modules to use
            memcpy(&last_command, rx_buffer, sizeof(last_command));
            
            if (!connected) {
                connected = true;
                Serial.println("[SPI] Connection established");
            }
        } else {
            static uint32_t last_ver_err = 0;
            if (millis() - last_ver_err > 1000) {
                last_ver_err = millis();
                Serial.printf("[SPI] Version mismatch: got %d, expected %d\n",
                    cmd->version, PROTOCOL_VERSION);
            }
        }
    } else {
        static uint32_t last_chk_err = 0;
        if (millis() - last_chk_err > 1000) {
            last_chk_err = millis();
            Serial.printf("[SPI] RX checksum fail: ver=%d, byte0=0x%02X\n",
                rx_buffer[0], rx_buffer[1]);
        }
    }
    
    // Prepare next transaction with updated status
    memcpy(tx_buffer, &current_status, sizeof(current_status));
    protocolSign(tx_buffer, sizeof(current_status));
    
    // Re-queue for next transaction
    memset(&slave_trans, 0, sizeof(slave_trans));
    slave_trans.length = PROTOCOL_PACKET_SIZE * 8;
    slave_trans.rx_buffer = rx_buffer;
    slave_trans.tx_buffer = tx_buffer;
    spi_slave_queue_trans(SPI2_HOST, &slave_trans, portMAX_DELAY);
}

void SpiSlave::setStatus(const StatusPacket& status) {
    // Copy status (will be sent on next transaction)
    current_status = status;
    current_status.version = PROTOCOL_VERSION;
    
    // Also update the TX buffer immediately so it's ready for the next transaction
    // This helps ensure the slave always has valid data to send
    memcpy(tx_buffer, &current_status, sizeof(current_status));
    protocolSign(tx_buffer, sizeof(current_status));
}
