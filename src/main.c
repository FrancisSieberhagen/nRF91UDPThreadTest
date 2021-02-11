/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <modem/lte_lc.h>
#include <net/socket.h>

#include <drivers/gpio.h>

#include "cJSON.h"

#define IP_HEADER_SIZE 28

static int client_fd;
static struct sockaddr_storage host_addr;
static struct k_delayed_work server_transmission_work;

K_SEM_DEFINE(lte_connected, 0, 1);

#if defined(CONFIG_BSD_LIBRARY)

#define LED_PORT        DT_GPIO_LABEL(DT_ALIAS(led0), gpios)
#define LED1	 DT_GPIO_PIN(DT_ALIAS(led0), gpios)
#define LED2	 DT_GPIO_PIN(DT_ALIAS(led1), gpios)
#define LED3	 DT_GPIO_PIN(DT_ALIAS(led2), gpios)
#define LED4	 DT_GPIO_PIN(DT_ALIAS(led3), gpios)

struct device *led_device;

static void init_led()
{

    led_device = device_get_binding(LED_PORT);

    /* Set LED pin as output */
    gpio_pin_configure(led_device, DT_GPIO_PIN(DT_ALIAS(led0), gpios),
                       GPIO_OUTPUT_ACTIVE |
                       DT_GPIO_FLAGS(DT_ALIAS(led0), gpios));
    gpio_pin_configure(led_device, DT_GPIO_PIN(DT_ALIAS(led1), gpios),
                       GPIO_OUTPUT_ACTIVE |
                       DT_GPIO_FLAGS(DT_ALIAS(led1), gpios));
    gpio_pin_configure(led_device, DT_GPIO_PIN(DT_ALIAS(led2), gpios),
                       GPIO_OUTPUT_ACTIVE |
                       DT_GPIO_FLAGS(DT_ALIAS(led2), gpios));
    gpio_pin_configure(led_device, DT_GPIO_PIN(DT_ALIAS(led3), gpios),
                       GPIO_OUTPUT_ACTIVE |
                       DT_GPIO_FLAGS(DT_ALIAS(led3), gpios));


}


static void led_on(char led)
{
    gpio_pin_set(led_device, led, 1);
}
static void led_off(char led)
{
    gpio_pin_set(led_device, led, 0);

}

static void led_on_off(char led, bool on_off)
{
    if (on_off)
    {
        led_on(led);
    } else {
        led_off(led);
    }
}

#endif

bool led_toggle = false;

char* create_json_msg() {



    cJSON *monitor = cJSON_CreateObject();

    cJSON *name = cJSON_CreateString("BSD Test");
    led_toggle = !led_toggle;
    cJSON *led1 = cJSON_CreateBool(led_toggle);
    cJSON *led2 = cJSON_CreateBool(!led_toggle);

    cJSON_AddItemToObject(monitor, "ActionName", name);
    cJSON_AddItemToObject(monitor, "LED1", led1);
    cJSON_AddItemToObject(monitor, "LED2", led2);

    char *string = cJSON_PrintUnformatted(monitor);

    cJSON_Delete(monitor);

    return string;
}

static void action_json_msg(char *msgbuf) {

    cJSON *monitor_json = cJSON_Parse(msgbuf);

    if (monitor_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printk("ERROR: cJSON Parse : %s\n", error_ptr);
            return;
        }
    }

    cJSON *value_name = cJSON_GetObjectItemCaseSensitive(monitor_json, "ActionName");
    if (cJSON_IsString(value_name) && (value_name->valuestring != NULL))
    {
        if (strcmp((value_name->valuestring),"BSD Test") == 0) {
            cJSON *value_led1 = cJSON_GetObjectItemCaseSensitive(monitor_json, "LED1");
            if (cJSON_IsString(value_name) && (value_name->valuestring != NULL)) {
                #if defined(CONFIG_BSD_LIBRARY)
                led_on_off(LED3, value_led1->valueint);
                #else
                printk("LED1 = %d\n", value_led1->valueint);
                #endif
            }
            cJSON *value_led2 = cJSON_GetObjectItemCaseSensitive(monitor_json, "LED2");
            if (cJSON_IsString(value_name) && (value_name->valuestring != NULL)) {
                #if defined(CONFIG_BSD_LIBRARY)
                led_on_off(LED4, value_led2->valueint);
                #else
                printk("LED2 = %d\n", value_led2->valueint);
                #endif
            }
        }
    }

    cJSON_Delete(monitor_json);
}


static void server_transmission_work_fn(struct k_work *work)
{
	int err, recsize;
	char buffer[CONFIG_DATA_UPLOAD_SIZE_BYTES + 1] = {"\0"};

  
        char msgbuf[100];
        char *string = create_json_msg();


	strcpy(buffer, string);

	printk("Transmitting UDP/IP payload of %d bytes to the ",
	       CONFIG_DATA_UPLOAD_SIZE_BYTES + IP_HEADER_SIZE);
	printk("IP address %s, port number %d\n",
	       CONFIG_SERVER_HOST,
	       CONFIG_SERVER_PORT);

	err = send(client_fd, buffer, CONFIG_DATA_UPLOAD_SIZE_BYTES, 0);
	if (err < 0) {
		printk("Failed to transmit UDP packet, %d\n", errno);
		return;
	}

        recsize = recv(client_fd, msgbuf, sizeof msgbuf, 0);
        if (recsize > 0) {
                printk("Received UDP/IP payload of %d bytes data:[%s]\n",recsize, msgbuf);

                action_json_msg(msgbuf);
        } else if (recsize == -1) {
                printk("Received packet time out\n");
        }
	k_delayed_work_submit(
			&server_transmission_work,
			K_SECONDS(CONFIG_DATA_UPLOAD_FREQUENCY_SECONDS));
}

static void work_init(void)
{
	k_delayed_work_init(&server_transmission_work,
			    server_transmission_work_fn);
}

#if defined(CONFIG_BSD_LIBRARY)
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		     (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			break;
		}

		printk("Network registration status: %s\n",
			evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
			"Connected - home network" : "Connected - roaming\n");
		k_sem_give(&lte_connected);
		break;
	case LTE_LC_EVT_PSM_UPDATE:
		printk("PSM parameter update: TAU: %d, Active time: %d\n",
			evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;
	case LTE_LC_EVT_EDRX_UPDATE: {
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
			       "eDRX parameter update: eDRX: %f, PTW: %f\n",
			       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0) {
			printk("%s\n", log_buf);
		}
		break;
	}
	case LTE_LC_EVT_RRC_UPDATE:
		printk("RRC mode: %s\n",
			evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
			"Connected" : "Idle\n");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
		       evt->cell.id, evt->cell.tac);
		break;
	default:
		break;
	}
}

static int configure_low_power(void)
{
	int err;

#if defined(CONFIG_PSM_ENABLE)
	/** Power Saving Mode */
	err = lte_lc_psm_req(true);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#else
	err = lte_lc_psm_req(false);
	if (err) {
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_EDRX_ENABLE)
	/** enhanced Discontinuous Reception */
	err = lte_lc_edrx_req(true);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#else
	err = lte_lc_edrx_req(false);
	if (err) {
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_RAI_ENABLE)
	/** Release Assistance Indication  */
	err = lte_lc_rai_req(true);
	if (err) {
		printk("lte_lc_rai_req, error: %d\n", err);
	}
#endif

	return err;
}

static void modem_configure(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already configured and LTE connected. */
	} else {
		err = lte_lc_init_and_connect_async(lte_handler);
		if (err) {
			printk("Modem configuration, error: %d\n", err);
			return;
		}
	}
}
#endif

static void server_disconnect(void)
{
	(void)close(client_fd);
}

static int server_init(void)
{
	struct sockaddr_in *server4 = ((struct sockaddr_in *)&host_addr);

	server4->sin_family = AF_INET;
	server4->sin_port = htons(CONFIG_SERVER_PORT);

	inet_pton(AF_INET, CONFIG_SERVER_HOST,
		  &server4->sin_addr);

	return 0;
}

static int server_connect(void)
{
	int err;

        struct timeval timeout;

        timeout.tv_sec = 2;
        timeout.tv_usec = 500000;  // 2.5 seconds


	client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client_fd < 0) {
		printk("Failed to create UDP socket: %d\n", errno);
		goto error;
	}

	err = connect(client_fd, (struct sockaddr *)&host_addr,
		      sizeof(struct sockaddr_in));
	if (err < 0) {
		printk("Connect failed : %d\n", errno);
		goto error;
	}

#if defined(CONFIG_BSD_LIBRARY)   // qemu_x86 returns -1
        err = setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof timeout);
        if (err < 0) {
                printk("Set timout error %d\n", err);
        }
#endif

	return 0;

error:
	server_disconnect();

	return err;
}

void main(void)
{
	int err;

	printk("UDP V1.0 sample has started\n");

	work_init();

#if defined(CONFIG_BSD_LIBRARY)
	err = configure_low_power();
	if (err) {
		printk("Unable to set low power configuration, error: %d\n",
		       err);
	}

	modem_configure();


        init_led();

	k_sem_take(&lte_connected, K_FOREVER);
#endif

	err = server_init();
	if (err) {
		printk("Not able to initialize UDP server connection\n");
		return;
	}

	err = server_connect();
	if (err) {
		printk("Not able to connect to UDP server\n");
		return;
	}

	k_delayed_work_submit(&server_transmission_work, K_NO_WAIT);
}
