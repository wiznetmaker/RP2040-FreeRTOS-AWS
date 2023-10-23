/**
 * Copyright (c) 2021 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>

#include "port_common.h"
#include "pico/stdlib.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "wizchip_conf.h"
#include "w5x00_spi.h"

#include "dhcp.h"
#include "timer.h"
#include "timers.h"

#include "mqtt_transport_interface.h"
#include "ssl_transport_interface.h"
#include "timer_interface.h"

#include "mqtt_certificate.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Task */
#define MQTT_TASK_STACK_SIZE 2048 * 2
#define MQTT_TASK_PRIORITY 10

#define YIELD_TASK_STACK_SIZE 512
#define YIELD_TASK_PRIORITY 8

/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_MQTT 0
#define SOCKET_DHCP 1
// socket number 3 is used in dns interface

/* Port */
#define TARGET_PORT 8883

/* MQTT */
#define MQTT_PUB_PERIOD (1000 * 5) // 5 seconds

/* AWS IoT */
// #define MQTT_DOMAIN "account-specific-prefix-ats.iot.ap-northeast-2.amazonaws.com"
#define MQTT_DOMAIN "a3uz5t2azg1xdz-ats.iot.ap-northeast-2.amazonaws.com"
#define MQTT_PUB_TOPIC "$aws/things/louis_thing/shadow/update"
#define MQTT_SUB_TOPIC "$aws/things/louis_thing/shadow/update/accepted"
#define MQTT_USERNAME NULL
#define MQTT_PASSWORD NULL
#define MQTT_CLIENT_ID "louis_test"

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 11, 2},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 11, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_DHCP                         // DHCP
};
static uint8_t g_ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
}; // common buffer

/* MQTT */
static uint8_t g_mqtt_buf[MQTT_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_mqtt_pub_msg_buf[MQTT_BUF_MAX_SIZE] = {
    0,
};

/* SSL */
tlsContext_t g_mqtt_tls_context;

/* Semaphore Handle */
SemaphoreHandle_t publishSemaphore;
// TaskHandle_t publishHandle, yieldHandle;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Task */
void aws_mqtt_task(void *argument);
void aws_yield_task(void *argument);
// void wizchip_dhcp_task(void *argument);
// void vStatusCheckTask(void *argument);

/* Clock */
static void set_clock_khz(void);

/* DHCP */
static void wizchip_dhcp_init(void);
static void wizchip_dhcp_assign(void);
static void wizchip_dhcp_conflict(void);


/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
int main()
{
    /* Initialize */
    set_clock_khz();

    stdio_init_all();

    // wizchip_delay_ms(1000 * 3); // wait for 3 seconds

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    wizchip_1ms_timer_initialize(repeating_timer_callback);

    publishSemaphore = xSemaphoreCreateMutex();

    xTaskCreate(aws_mqtt_task, "MQTT_Task", MQTT_TASK_STACK_SIZE, NULL, MQTT_TASK_PRIORITY, NULL);
    xTaskCreate(aws_yield_task, "YIELD_Task", YIELD_TASK_STACK_SIZE, NULL, YIELD_TASK_PRIORITY, NULL);

    vTaskStartScheduler();

    /* Infinite loop */
    while (1)
    {
        ;
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

/* DHCP */
static void wizchip_dhcp_init(void)
{
    printf(" DHCP client running\n");

    DHCP_init(SOCKET_DHCP, g_ethernet_buf);

    reg_dhcp_cbfunc(wizchip_dhcp_assign, wizchip_dhcp_assign, wizchip_dhcp_conflict);
}

static void wizchip_dhcp_assign(void)
{
    getIPfromDHCP(g_net_info.ip);
    getGWfromDHCP(g_net_info.gw);
    getSNfromDHCP(g_net_info.sn);
    getDNSfromDHCP(g_net_info.dns);

    g_net_info.dhcp = NETINFO_DHCP;

    /* Network initialize */
    network_initialize(g_net_info); // apply from DHCP

    print_network_information(g_net_info);
    printf(" DHCP leased time : %ld seconds\n", getDHCPLeasetime());
}

static void wizchip_dhcp_conflict(void)
{
    printf(" Conflict IP from DHCP\n");

    // halt or reset or any...
    while (1)
        ; // this example is halt.
}


/* MQTT task */
void aws_mqtt_task(void *argument)
{
    /* Initialize */
    int retval = 0;
    uint32_t tick_start = 0;
    uint32_t tick_end = 0;
    uint32_t pub_cnt = 0;

    if (g_net_info.dhcp == NETINFO_DHCP) // DHCP
    {
        wizchip_dhcp_init();

        while (1)
        {
            retval = DHCP_run();

            if (retval == DHCP_IP_LEASED)
            {
                break;
            }

            wizchip_delay_ms(1000);
        }
    }
    else // static
    {
        network_initialize(g_net_info);
        print_network_information(g_net_info);
    }

    /* Setup certificate */
    g_mqtt_tls_context.rootca_option = MBEDTLS_SSL_VERIFY_REQUIRED; // use Root CA verify
    g_mqtt_tls_context.clica_option = 1;                            // use client certificate
    g_mqtt_tls_context.root_ca = mqtt_root_ca;
    g_mqtt_tls_context.client_cert = mqtt_client_cert;
    g_mqtt_tls_context.private_key = mqtt_private_key;

    retval = mqtt_transport_init(true, MQTT_CLIENT_ID, NULL, NULL, MQTT_KEEP_ALIVE);
    if (retval != 0)
    {
        printf(" Failed, mqtt_transport_init returned %d\n", retval);

        while (1)
            ;
    }

    retval = mqtt_transport_connect(SOCKET_MQTT, 1, g_mqtt_buf, MQTT_BUF_MAX_SIZE, MQTT_DOMAIN, TARGET_PORT, &g_mqtt_tls_context);
    if (retval != 0)
    {
        printf(" Failed, mqtt_transport_connect returned %d\n", retval);

        while (1)
            ;
    }

    retval = mqtt_transport_subscribe(0, MQTT_SUB_TOPIC);
    if (retval != 0)
    {
        printf(" Failed, mqtt_transport_subscribe returned %d\n", retval);

        while (1)
            ;
    }

    xSemaphoreGive(publishSemaphore);
    while(1)
    {
        sprintf(g_mqtt_pub_msg_buf, "{\"message\":\"Hello, World!\", \"publish count\":\"%d\"}\n", pub_cnt++);
        retval = mqtt_transport_publish(MQTT_PUB_TOPIC, g_mqtt_pub_msg_buf, strlen(g_mqtt_pub_msg_buf), 0);
        
        if (retval != 0)
        {
            printf("Publish failed : %d\n", retval);
            while(1)
            {
                vTaskDelay(1000);
            }
        }
        vTaskDelay(MQTT_PUB_PERIOD); //5sec Delay
    }
}

void aws_yield_task(void *argument)
{
    /* Initialize */
    int retval = 0;
    uint32_t pub_count = 0;
    uint32_t pub_cnt = 0;

    xSemaphoreTake(publishSemaphore, portMAX_DELAY);
    while(1)
    {
        retval = mqtt_transport_yield(MQTT_YIELD_TIMEOUT);
        if (retval != 0)
        {
            printf(" Failed, mqtt_transport_yield returned %d\n", retval);

            while (1)
            {
                vTaskDelay(100);
            }
        }
        vTaskDelay(10);
    }
}


/* MQTT task */
//@TODO phy링크 확인 후 DHCP 동작 task 작성
/*
void wizchip_dhcp_task(void *argument)
{

}
*/
