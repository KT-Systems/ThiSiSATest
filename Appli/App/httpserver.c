/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    httpserver.c
  * @author  GPM Application Team
  * @brief   Http server application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
#if 0
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "httpserver.h"
#include "main.h"

#include "w6x_api.h"
#include "common_parser.h" /* Common Parser functions */

/* Please read the following Wiki on how to create the html_pages.h binary:
https://wiki.st.com/stm32mcu/wiki/Connectivity:Wi-Fi_ST67W6X_HTTP_Server_Application */
#include "html_pages.h"

#include "FreeRTOS.h"
#include "event_groups.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Global variables ----------------------------------------------------------*/
/* USER CODE BEGIN GV */

/* USER CODE END GV */

/* Private typedef -----------------------------------------------------------*/
/**
  * @brief  HTTP server response type
  */
typedef enum
{
  FAVICON_SVG,
  ST_LOGO_SVG,
  INDEX_HTML,
  LED_GREEN_STATE,
  LED_RED_STATE,
  BUTTON_STATE,
  ERROR_404_HTML,
  UNKNOWN_RESPONSE
} HttpServer_response_e;

/**
  * @brief  HTTP server response structure
  */
typedef struct
{
  HttpServer_response_e response_type;  /*!< Type of response */
  const char *request;                  /*!< Request string */
  const char *response;                 /*!< Response string */
} HttpServer_response_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
/** Priority of the user pins polling task */
#define PIN_POLLING_THREAD_PRIO        30

/** Stack size of the user pins polling task */
#define PIN_POLLING_TASK_STACK_SIZE    512

/** Priority of the web server child task */
#define WEBSERVER_CHILD_THREAD_PRIO    29

/** Stack size of the web server child task */
#define HTTP_CHILD_TASK_STACK_SIZE     2048

/** HTTP server port */
#define HTTP_PORT                      80

/** Socket timeout in ms */
#define SOCKET_TIMEOUT_MS              1000

/** Event flag for pin status update */
#define EVENT_FLAG_PIN                 (1<<1)

/** Timeout for the pin status update in ms */
#define PIN_TIMEOUT_MS                 9000

/** Buffer size for the child task */
#define HTTP_CHILD_TASK_BUFFER_SIZE    1024

/** Maximum bytes to send in one step */
#define MAX_BYTES_TO_SEND              4096

/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/** Button and Leds states */
static volatile PinInfos_t pins_info =
{
  .btn_state = GPIO_PIN_RESET,
  .led_green_state = GPIO_PIN_RESET,
  .led_red_state = GPIO_PIN_RESET
};

/** Event group handle for pin status update */
EventGroupHandle_t pin_handle;

/** Flag to indicate if the button state has changed */
extern uint8_t button_changed;

/** Response with OK content */
static const char response_ok_html[] =
{
  "HTTP/1.1 200 OK\r\n"
  "Server: U5\r\n"
  "Access-Control-Allow-Origin: * \r\n"
  "Cache-Control: no-cache\r\n"
  "Keep-Alive: timeout=2, max=2\r\n"
  "Connection: close\r\n"
  "Content-Type: text/html; charset=utf-8\r\n"
  "Content-Length: 2\r\n\r\n"
  "OK"
};

/** Response content depending on the request */
HttpServer_response_t http_server_responses[] =
{
  {INDEX_HTML,      "GET / ",                                     response_index_html},
  {FAVICON_SVG,     "GET /favicon.ico",                           response_favicon_svg},
  {ST_LOGO_SVG,     "GET /ST_logo_2020_white_no_tagline_rgb.svg", response_st_logo_svg},
  {LED_GREEN_STATE, "GET /LedGreen",                              response_ok_html},
  {LED_RED_STATE,   "GET /LedRed",                                response_ok_html},
  {BUTTON_STATE,    "GET /pins_status",                           NULL},
};

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  Web server child task
  * @param  arg: pointer on argument(not used here)
  */
static void http_server_serve_task(void *arg);

/**
  * @brief  Pins polling task
  * @param  arg: pointer on argument(not used here)
  */
static void pin_verification_task(void *arg);

/**
  * @brief  Write HTML data to client
  * @param  client: client information
  * @param  buffer: HTML data to send
  * @param  buffer_size: size of the data to send
  * @retval 0 if success, 1 otherwise
  */
static int32_t http_server_write(int32_t client, const char *buffer, size_t buffer_size);

/**
  * @brief  Close client connection
  * @param  client: client information
  * @retval 0
  */
static int32_t close_client(int32_t client);

/**
  * @brief  Process the HTTP response
  * @param  client: client information
  * @param  recv_buffer: received request data
  */
static void http_process_response(int32_t client, char *recv_buffer);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/
void http_server_socket(void *arg)
{
  (void)arg;
  const int32_t domain = AF_INET;
  int32_t sock = -1;
  uint16_t port = HTTP_PORT;
  struct sockaddr_in s_addr_in_t = { 0 };
  uint8_t ip_addr[4] = {0};
  uint8_t netmask_addr[4] = {0};
  int32_t timeout = (int32_t)pdMS_TO_TICKS(SOCKET_TIMEOUT_MS);
  int32_t fct_start = 9;

  /* USER CODE BEGIN http_server_socket_1 */

  /* USER CODE END http_server_socket_1 */

  /* Get the soft-AP current IP address */
  if (W6X_Net_AP_GetIPAddress(ip_addr, netmask_addr) != W6X_STATUS_OK)
  {
    LogError("Get soft-AP IP failed\n");
    goto _err;
  }

  LogInfo("Soft-AP IP address : " IPSTR "\n", IP2STR(ip_addr));
  s_addr_in_t.sin_addr.s_addr = ATON(ip_addr);
  s_addr_in_t.sin_port = PP_HTONS(port);
  s_addr_in_t.sin_family = AF_INET;

  /* Create a new TCP server socket with port HTTP_PORT */
  sock = W6X_Net_Socket(domain, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
  {
    LogInfo("Could not create the socket.\n");
    goto _err;
  }

  /* Set the socket Receive timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)) != 0)
  {
    LogError("Could not set the socket options.\n");
    goto _err;
  }

  /* Set the socket Send timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout)) != 0)
  {
    LogError("Could not set the socket options.\n");
    goto _err;
  }

  /* Set the socket Send timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&timeout, sizeof(timeout)) != 0)
  {
    LogError("Could not set the socket options.\n");
    goto _err;
  }

  /* Bind the socket to the server address */
  if (W6X_Net_Bind(sock, (struct sockaddr *)&s_addr_in_t, sizeof(s_addr_in_t)) != 0)
  {
    LogInfo("\n LwIP Bind Fail\n");
    goto _err;
  }

  LogInfo("\n LwIP Bind Pass\n");

  /* Listen for incoming connections (TCP listen backlog = 5). */
  if (W6X_Net_Listen(sock, 5) != 0)
  {
    LogInfo("\n LwIP Listen Fail\n");
    goto _err;
  }

  LogInfo("\n LwIP Listen Pass\n");

  /* USER CODE BEGIN http_server_socket_2 */

  /* USER CODE END http_server_socket_2 */

  /* Creation of a thread to check if the pin value of the button or the LEDs has change */
  pin_handle = xEventGroupCreate();
  if (pdPASS != xTaskCreate((TaskFunction_t)pin_verification_task, "UserPinsPolling",
                            PIN_POLLING_TASK_STACK_SIZE >> 2,
                            &fct_start, PIN_POLLING_THREAD_PRIO, NULL))
  {
    LogInfo("User pins task creation failed\n");
    goto _err;
  }

  while (1)
  {
    struct sockaddr remotehost_t;
    uint32_t remotehost_size = sizeof(remotehost_t);

    /* Wait for an incoming client connection */
    int32_t newconn = W6X_Net_Accept(sock, (struct sockaddr *)&remotehost_t, (socklen_t *)&remotehost_size);
    if (newconn < 0)
    {
      vTaskDelay(200);
      LogInfo("\n Failed to accept new client requests.\n");
    }
    else
    {
      /* Create a temporary thread to process the incoming HTTP request */
      char thread_name[14];
      const size_t thread_name_len = sizeof(thread_name);
      snprintf(thread_name, thread_name_len, "HTTP_%08" PRIX32, newconn);
      thread_name[thread_name_len - 1] = '\0';

      LogDebug("\n Creation of temporary thread to process an incoming HTTP request : %" PRIi32 "\n", newconn);
      if (pdPASS != xTaskCreate((TaskFunction_t)http_server_serve_task, thread_name,
                                HTTP_CHILD_TASK_STACK_SIZE >> 2,
                                &newconn, WEBSERVER_CHILD_THREAD_PRIO, NULL))
      {
        LogInfo("%s task creation failed\n", thread_name);
      }
      /* Delay added to avoid that too many requests are processed in parallel */
      vTaskDelay(50);
    }
  }

  /* USER CODE BEGIN http_server_socket_last */

  /* USER CODE END http_server_socket_last */

_err:
  /* Error case */
  if ((sock >= 0) && (W6X_Net_Shutdown(sock, 1) != W6X_STATUS_OK))
  {
    LogError("Failed to close server socket\n");
  }
  return;
}

/* USER CODE BEGIN FD */

/* USER CODE END FD */

/* Private Functions Definition ----------------------------------------------*/
static void http_server_serve_task(void *arg)
{
  int32_t client = *((int32_t *)arg);
  int32_t bytes_received;
  int32_t recv_total_len = 0;
  /* Allocate considering the biggest buffer the client can send */
  size_t recv_buffer_len = HTTP_CHILD_TASK_BUFFER_SIZE;
  char *recv_buffer = (char *)pvPortMalloc(recv_buffer_len);
  if (recv_buffer == NULL)
  {
    LogError("Unable to allocate recv buffer\n");
    goto _err;
  }

  /* USER CODE BEGIN http_server_serve_task_1 */

  /* USER CODE END http_server_serve_task_1 */

  /* Read in the request */
  recv_buffer[0] = '\0';
  do
  {
    bytes_received = W6X_Net_Recv(client, (uint8_t *)&recv_buffer[recv_total_len], recv_buffer_len, 0);
    if (bytes_received < 0) /* No data received or error */
    {
      break;
    }

    /* Case where we have receive less than the buffer allocated */
    if (bytes_received < recv_buffer_len)
    {
      recv_total_len += bytes_received;
      break;
    }

    recv_total_len += bytes_received;
    recv_buffer_len -= bytes_received;
  } while ((bytes_received != 0) && (recv_buffer_len > 0));

  if (recv_total_len == 0)
  {
    LogError("No data read on the socket, closing the socket\n");
    goto _close;
  }
  LogDebug("\n %" PRIi32 " >>> %" PRIi32 " <<<\n", client, recv_total_len);

  /* Count can be negative. */
  if (recv_total_len > 0)
  {
    recv_buffer[recv_total_len++] = '\0';
    LogDebug("\n %" PRIi32 " >>> %s <<<\n", client, recv_buffer);
  }

  /* USER CODE BEGIN http_server_serve_task_2 */

  /* USER CODE END http_server_serve_task_2 */

  /* Process the response */
  http_process_response(client, recv_buffer);

  /* USER CODE BEGIN http_server_serve_task_last */

  /* USER CODE END http_server_serve_task_last */

_close:
  vPortFree(recv_buffer);
_err:
  close_client(client);
  vTaskDelete(NULL);
}

static void pin_verification_task(void *arg)
{
  UNUSED(arg);
  GPIO_PinState Btn;
  GPIO_PinState LedGreen;
  GPIO_PinState LedRed;

  /* USER CODE BEGIN pin_verification_task_1 */

  /* USER CODE END pin_verification_task_1 */

  while (1)
  {
    do /* Wait for the button or the LEDs to change */
    {
      vTaskDelay(50);
      Btn = (GPIO_PinState)button_changed;
      LedGreen = HAL_GPIO_ReadPin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
      LedRed = HAL_GPIO_ReadPin(LED_RED_GPIO_Port, LED_RED_Pin);
    } while ((Btn == pins_info.btn_state) &&
             (LedGreen == pins_info.led_green_state) &&
             (LedRed == pins_info.led_red_state));

    /* Update the button and the LEDs states */
    pins_info.btn_state = Btn;
    pins_info.led_green_state = LedGreen;
    pins_info.led_red_state = LedRed;

    /* Notify the HTTP server task */
    xEventGroupSetBits(pin_handle, EVENT_FLAG_PIN);
  }
}

static int32_t http_server_write(int32_t client, const char *buffer, size_t buffer_size)
{
  int32_t bytes_sent = 0;

  /* USER CODE BEGIN http_server_write_1 */

  /* USER CODE END http_server_write_1 */

  LogDebug("[%" PRIi32 "] *****> %s<*****\n", client, buffer);

  do /* Send the data. Can be done in multiple steps. */
  {
    bytes_sent = W6X_Net_Send(client, (void *)buffer, buffer_size, 0);
    if (bytes_sent < 0)
    {
      LogError("[%" PRIi32 "] *****> SEND ERROR <*****\n", client);
      return 1;
    }
    buffer_size -= bytes_sent;
    buffer += bytes_sent;
  } while (buffer_size > 0);

  /* USER CODE BEGIN http_server_write_last */

  /* USER CODE END http_server_write_last */

  return 0;
}

int32_t close_client(int32_t client)
{
  /* USER CODE BEGIN close_client_1 */

  /* USER CODE END close_client_1 */

  /* Close the connection */
  (void)W6X_Net_Close(client);
  LogDebug("!!! Closed connection <%" PRIi32 "> !!!\n", client);

  /* USER CODE BEGIN close_client_last */

  /* USER CODE END close_client_last */

  return 0;
}

static void http_process_response(int32_t client, char *recv_buffer)
{
  HttpServer_response_e response = UNKNOWN_RESPONSE;
  char *response_data = NULL;
  char response_template[250] =
  {
    "HTTP/1.1 200 OK\r\n"
    "Server: U5\r\n"
    "Access-Control-Allow-Origin: * \r\n"
    "Cache-Control: no-cache\r\n"
    "Keep-Alive: timeout=2, max=2\r\n"
    "Connection: close\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
  };

  /* USER CODE BEGIN http_process_response_1 */

  /* USER CODE END http_process_response_1 */

  /* Check the request to determine the response */
  for (uint32_t i = 0; i < sizeof(http_server_responses) / sizeof(http_server_responses[0]); i++)
  {
    if (strncmp(recv_buffer, http_server_responses[i].request, strlen(http_server_responses[i].request)) == 0)
    {
      response = http_server_responses[i].response_type;
      response_data = (char *)http_server_responses[i].response;
      break;
    }
  }

  /* USER CODE BEGIN http_process_response_2 */

  /* USER CODE END http_process_response_2 */

  if (response == UNKNOWN_RESPONSE) /* Request not recognized, return 404 error */
  {
    response_data = (char *)response_error_404_html;
  }
  else if (response == BUTTON_STATE) /* Prepare a custom response for the button state */
  {
    LogInfo("\nPending request for the PIN STATUS received\n\n");
    /* Wait for the pin status update */
    (void)xEventGroupWaitBits(pin_handle, EVENT_FLAG_PIN, pdTRUE, pdFALSE, pdMS_TO_TICKS(PIN_TIMEOUT_MS));
    /* Send anyway the pin status update or the last known status if timeouted */
    /* Prepare the HTTP content data */
    char data[60];
    snprintf(data, sizeof(data), "{\"LedGreenPin\":%" PRIu16 ",\"LedRedPin\":%" PRIu16 ",\"BtnPin\":%" PRIu16 "}",
             (pins_info.led_green_state == GPIO_PIN_RESET ? 0 : 1),
             (pins_info.led_red_state == GPIO_PIN_RESET ? 0 : 1),
             (pins_info.btn_state == GPIO_PIN_RESET ? 0 : 1));

    /* Append the content length and the data to the response */
    snprintf(&response_template[strlen(response_template)], sizeof(response_template) - strlen(response_template),
             "Content-Length: %" PRIu32 "\r\n\r\n%s", (uint32_t)strlen(data), data);

    LogInfo("Pins status :\n%s\n", data);
    response_data = response_template;
  }

  /* Send the response */
  if (1 == http_server_write(client, response_data, strlen(response_data)))
  {
    return;
  }

  /* Finalize the response */
  if (response == LED_RED_STATE) /* Toggle the red LED */
  {
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
  }
  else if (response == LED_GREEN_STATE) /* Toggle the green LED */
  {
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);
  }

  /* USER CODE BEGIN http_process_response_last */

  /* USER CODE END http_process_response_last */
}

/* USER CODE BEGIN PFD */
#endif


/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "httpserver.h"
#include "main.h"

#include "w6x_api.h"
#include "common_parser.h" /* Common Parser functions */

/* Please read the following Wiki on how to create the html_pages.h binary:
https://wiki.st.com/stm32mcu/wiki/Connectivity:Wifi_ST67W6X_HTTP_Server_Application */
#include "html_pages.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "cJSON.h"

extern FDCAN_HandleTypeDef hfdcan1;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;
extern uint32_t errorHandlerFlag;
uint8_t jsonCounter = 0;

/* Global variables ----------------------------------------------------------*/
typedef enum
{
  FAVICON_SVG,
  ST_LOGO_SVG,
  INDEX_HTML,
  LED_GREEN_STATE,
  LED_RED_STATE,
  BUTTON_STATE,
  DATA_STATE,
  CHARGE_COMMAND,
  ERROR_404_HTML,
  UNKNOWN_RESPONSE
} HttpServer_response_e;

/**
  * @brief  HTTP server response structure
  */
typedef struct
{
  HttpServer_response_e response_type;
  const char *request;
  const char *response;
} HttpServer_response_t;


/* Private defines -----------------------------------------------------------*/
/** Priority of the user pins polling task */
#define PIN_POLLING_THREAD_PRIO        30

/** Stack size of the user pins polling task */
#define PIN_POLLING_TASK_STACK_SIZE    512

/** Priority of the web server child task */
#define WEBSERVER_CHILD_THREAD_PRIO    29

/** Stack size of the web server child task */
#define HTTP_CHILD_TASK_STACK_SIZE     8192

/** HTTP server port */
#define HTTP_PORT                      80

/** Socket timeout in ms */
#define SOCKET_TIMEOUT_MS              1000

/** Event flag for pin status update */
#define EVENT_FLAG_PIN                 (1<<1)

/** Timeout for the pin status update in ms */
#define PIN_TIMEOUT_MS                 9000

/** Buffer size for the child task */
#define HTTP_CHILD_TASK_BUFFER_SIZE    1024

/** Maximum bytes to send in one step */
#define MAX_BYTES_TO_SEND              4096

extern float flag;
//static volatile PinInfosTypeDef pins_info =
//{
//  .btn_state = GPIO_PIN_RESET,
//  .led_green_state = GPIO_PIN_RESET,
//  .led_red_state = GPIO_PIN_RESET
//};

/** Event group handle for pin status update */
EventGroupHandle_t pin_handle;

/** Flag to indicate if the button state has changed */
extern uint8_t button_changed;

/** Response content depending on the request */
HttpServer_response_t http_server_responses[] =
{
  {DATA_STATE,       "GET /data",                                 NULL},
  {CHARGE_COMMAND,   "POST /charge",   						      NULL},
};

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  Web server child task
  * @param  arg: pointer on argument(not used here)
  */
static void http_server_serve_task(void *arg);

/**
  * @brief  Pins polling task
  * @param  arg: pointer on argument(not used here)
  */

/**
  * @brief  Write HTML data to client
  * @param  client: client information
  * @param  buffer: HTML data to send
  * @param  buffer_size: size of the data to send
  * @retval 0 if success, 1 otherwise
  */
static int32_t http_server_write(int32_t client, const char *buffer, size_t buffer_size);

/**
  * @brief  Close client connection
  * @param  client: client information
  * @retval 0
  */
static int32_t close_client(int32_t client);

/**
  * @brief  Process the HTTP response
  * @param  client: client information
  * @param  recv_buffer: received request data
  */
static void http_process_response(int32_t client, char *recv_buffer);

/* Functions Definition ------------------------------------------------------*/
void http_server_socket(void *arg)
{
  (void)arg;
  const int32_t domain = AF_INET;
  int32_t sock = -1;
  uint16_t port = HTTP_PORT;
  struct sockaddr_in s_addr_in_t = { 0 };
  uint8_t ip_addr[4] = {0};
  uint8_t netmask_addr[4] = {0};
  int32_t timeout = (int32_t)pdMS_TO_TICKS(SOCKET_TIMEOUT_MS);
  int32_t fct_start = 9;

  /* Get the soft-AP current IP address */
  if (W6X_WiFi_GetApIpAddress(ip_addr, netmask_addr) != W6X_STATUS_OK)
  {
    goto _err;
  }

  s_addr_in_t.sin_addr.s_addr = ATON_R(ip_addr);
  s_addr_in_t.sin_port = PP_HTONS(port);
  s_addr_in_t.sin_family = AF_INET;

  /* Create a new TCP server socket with port HTTP_PORT */
  sock = W6X_Net_Socket(domain, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
  {
    goto _err;
  }

  /* Set the socket Receive timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)) != 0)
  {
    goto _err;
  }

  /* Set the socket Send timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout)) != 0)
  {
    goto _err;
  }

  /* Set the socket Send timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&timeout, sizeof(timeout)) != 0)
  {
    goto _err;
  }

  /* Bind the socket to the server address */
  if (W6X_Net_Bind(sock, (struct sockaddr *)&s_addr_in_t, sizeof(s_addr_in_t)) != 0)
  {
    goto _err;
  }


  /* Listen for incoming connections (TCP listen back//Log = 5). */
  if (W6X_Net_Listen(sock, 5) != 0)
  {
    goto _err;
  }



  while (1)
  {
    struct sockaddr remotehost_t;
    uint32_t remotehost_size = sizeof(remotehost_t);

    /* Wait for an incoming client connection */
    int32_t newconn = W6X_Net_Accept(sock, (struct sockaddr *)&remotehost_t, (socklen_t *)&remotehost_size);
    if (newconn < 0)
    {
      vTaskDelay(200);
    }
    else
    {
      /* Create a temporary thread to process the incoming HTTP request */
      char thread_name[14];
      const size_t thread_name_len = sizeof(thread_name);
      snprintf(thread_name, thread_name_len, "HTTP_%08" PRIX32, newconn);
      thread_name[thread_name_len - 1] = '\0';

      if (pdPASS != xTaskCreate((TaskFunction_t)http_server_serve_task, thread_name,
                                HTTP_CHILD_TASK_STACK_SIZE >> 2,
                                &newconn, WEBSERVER_CHILD_THREAD_PRIO, NULL))
      {
      }
    }
  }


_err:
  /* Error case */
  if ((sock >= 0) && (W6X_Net_Shutdown(sock, 1) != W6X_STATUS_OK))
  {
  }
  return;
}

uint16_t currentResponse = 0;
uint16_t voltages[12][12];
uint8_t temperatures[12][10];
float icTemps[12];
uint8_t cellBalancing[12][12];
float ivtCurrent;
float currentCounter;
float ivtVoltage;
float wattage;
float wattageCounter;
float chargerVoltage;
float chargerCurrent;
float chargerErrors;
float ACUFlags;
float ACUHumidity;
float ACUTemperature;
float chargePost = 0;
float BMSFlags;

float imdResistance;
float imdFrequency;

float vicorCurrentIn;
float vicorCurrentOut;
float vicorVoltageIn;
float vicorVoltageOut;
float vicorTemperature;


float currentInputData = 0;
uint8_t chargingState = 0;
char *password;
uint8_t balancingState = 0;

/* Private Functions Definition ----------------------------------------------*/
static void http_server_serve_task(void *arg)
{
  int32_t client = *((int32_t *)arg);
  int32_t bytes_received;
  int32_t recv_total_len = 0;
  /* Allocate considering the biggest buffer the client can send */
  size_t recv_buffer_len = HTTP_CHILD_TASK_BUFFER_SIZE;
  char *recv_buffer = (char *)pvPortMalloc(recv_buffer_len);
  if (recv_buffer == NULL)
  {
    goto _err;
  }
  /* Read in the request */
  recv_buffer[0] = '\0';
  do
  {
    bytes_received = W6X_Net_Recv(client, (uint8_t *)&recv_buffer[recv_total_len], recv_buffer_len, 0);
    if (bytes_received < 0) /* No data received or error */
    {
      break;
    }

    /* Case where we have receive less than the buffer allocated */
    if (bytes_received < recv_buffer_len)
    {
      recv_total_len += bytes_received;
      break;
    }

    recv_total_len += bytes_received;
    recv_buffer_len -= bytes_received;
  } while ((bytes_received != 0) && (recv_buffer_len > 0));

  if (recv_total_len == 0)
  {
    goto _close;
  }

  /* Count can be negative. */
  if (recv_total_len > 0)
  {
    recv_buffer[recv_total_len++] = '\0';
  }


  /* Process the response */
  http_process_response(client, recv_buffer);


_close:
  vPortFree(recv_buffer);
_err:
  close_client(client);
  vTaskDelete(NULL);
}

#define W6X_MAX_PAYLOAD 500
//static int32_t http_server_write(int32_t client, const char *buffer, size_t buffer_size)
//{
//
//  size_t remaining = buffer_size;
//  uint8_t* p = buffer;
//  do {
//      size_t chunk = remaining > 450 ? 450 : remaining;
//      int bytes_sent = W6X_Net_Send(client, p, (int)chunk, 0);
//      if (bytes_sent < 0) {
//          // handle EAGAIN vs. fatal error as above
//          return 1;
//      }
//      if (bytes_sent == 0) {
//          // connection closed
//          return 1;
//      }
//      remaining -= (size_t)bytes_sent;
//      p += bytes_sent;
//  } while (remaining > 0);
//  return 0;
//}

static int32_t http_server_write(int32_t client, const char *buffer, size_t buffer_size)
{
  int32_t bytes_sent = 0;

  /* USER CODE BEGIN http_server_write_1 */

  /* USER CODE END http_server_write_1 */

  LogDebug("[%" PRIi32 "] *****> %s<*****\n", client, buffer);

  do /* Send the data. Can be done in multiple steps. */
  {
    bytes_sent = W6X_Net_Send(client, (void *)buffer, buffer_size, 0);
    if (bytes_sent < 0)
    {
      LogError("[%" PRIi32 "] *****> SEND ERROR <*****\n", client);
      return 1;
    }
    buffer_size -= bytes_sent;
    buffer += bytes_sent;
  } while (buffer_size > 0);

  /* USER CODE BEGIN http_server_write_last */

  /* USER CODE END http_server_write_last */

  return 0;
}

int32_t close_client(int32_t client)
{
  (void)W6X_Net_Close(client);
  return 0;
}

static void http_process_response(int32_t client, char *recv_buffer)
{
  HttpServer_response_e response = UNKNOWN_RESPONSE;
  char *response_data = NULL;
  char response_template[4000] =
  {
    "HTTP/1.1 200 OK\r\n"
    "Server: U5\r\n"
    "Access-Control-Allow-Origin: * \r\n"
    "Cache-Control: no-cache\r\n"
    "Keep-Alive: timeout=5, max=5\r\n"
    "Connection: close\r\n"
    "Content-Type: application/json; charset=utf-8\r\n"
  };

  for (uint32_t i = 0; i < sizeof(http_server_responses) / sizeof(http_server_responses[0]); i++)
  {
    if (strncmp(recv_buffer, http_server_responses[i].request, strlen(http_server_responses[i].request)) == 0)
    {
      response = http_server_responses[i].response_type;
      response_data = (char *)http_server_responses[i].response;
      break;
    }
  }

  if (response == UNKNOWN_RESPONSE) /* Request not recognized, return 404 error */
  {
    response_data = (char *)response_error_404_html;
  }
  else if(response == DATA_STATE){

//	  	 bms.wifiIsConnected = CONNECTED;
//	  	 bms.stopDisplayingTimer->Instance->CNT = TIMER_RESET;

		  cJSON_Hooks hooks =
		  {
			.malloc_fn = pvPortMalloc,
			.free_fn = vPortFree,
		  };
		  cJSON_InitHooks(&hooks);
//
		  cJSON *root = cJSON_CreateObject();
				{
			  // DASHBOARD
			  	  	    cJSON *mainDataJSON = cJSON_CreateObject();
		 	 			cJSON_AddItemToObject(root, "main", mainDataJSON);

		 	 			 cJSON_AddItemToObject(mainDataJSON, "iv", cJSON_CreateNumber(ivt.voltage1));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "mt", cJSON_CreateNumber(inverter.motorTemp));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "ds", cJSON_CreateNumber(dataLogger.speed));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "hct", cJSON_CreateNumber(acu.highestCellTemp));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "ic", cJSON_CreateNumber((float)ivt.current));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "it", cJSON_CreateNumber(inverter.igbtTemp));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "iw", cJSON_CreateNumber(ivt.wattage));
		 	 			 cJSON_AddItemToObject(mainDataJSON, "b", cJSON_CreateNumber(100.0 * ivt.voltage1 / MAX_PACK_VOLTAGE));

				  	  	cJSON *elecDataJSON = cJSON_CreateObject();
			 	 		cJSON_AddItemToObject(root, "elec", elecDataJSON);

			 	 		 cJSON_AddItemToObject(elecDataJSON, "iac", cJSON_CreateNumber(inverter.actualCurrent));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "ir", cJSON_CreateNumber(acu.IMDResistance));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "ic", cJSON_CreateNumber((float)ivt.current));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "icc", cJSON_CreateNumber(ivt.currentCounter));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "iw", cJSON_CreateNumber(ivt.wattage));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "iwc", cJSON_CreateNumber(ivt.wattageCounter));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "mnv", cJSON_CreateNumber(acu.minVoltageCell.val * 10));
			 	 		 cJSON_AddItemToObject(elecDataJSON, "mxv", cJSON_CreateNumber(acu.maxVoltageCell.val * 10));

			 	 		cJSON *mechDataJSON = cJSON_CreateObject();
			 	 		cJSON_AddItemToObject(root, "mech", mechDataJSON);

			 	 		 cJSON_AddItemToObject(mechDataJSON, "fl", cJSON_CreateNumber(dataLogger.frontLeftWheelRPM));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "fr", cJSON_CreateNumber(dataLogger.frontRightWheelRPM));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "rl", cJSON_CreateNumber(dataLogger.rearLeftWheelRPM));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "rr", cJSON_CreateNumber(dataLogger.rearRightWheelRPM));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "h", cJSON_CreateNumber(0));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "vbp", cJSON_CreateNumber( vcu.brakePressure));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "rpm", cJSON_CreateNumber(inverter.RPMSpeed));
			 	 		 cJSON_AddItemToObject(mechDataJSON, "s", cJSON_CreateNumber(dataLogger.speed));


			 	 		cJSON *tempDataJSON = cJSON_CreateObject();
			 	 		cJSON_AddItemToObject(root, "temp", tempDataJSON);

			 	 		 cJSON_AddItemToObject(tempDataJSON, "mt", cJSON_CreateNumber(inverter.motorTemp));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "it", cJSON_CreateNumber(inverter.igbtTemp));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "hct", cJSON_CreateNumber(acu.highestCellTemp));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "ct1", cJSON_CreateNumber(dataLogger.coolingThermistors[4]));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "ct2", cJSON_CreateNumber(dataLogger.coolingThermistors[1]));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "ct3", cJSON_CreateNumber(dataLogger.coolingThermistors[3]));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "vt", cJSON_CreateNumber(acu.vicorTemperature));
			 	 		 cJSON_AddItemToObject(tempDataJSON, "pt", cJSON_CreateNumber(acu.PCBTemperature));

			 	 		cJSON *debugDataJSON = cJSON_CreateObject();
			 	 		cJSON_AddItemToObject(root, "debug", debugDataJSON);

			 	 		 cJSON_AddItemToObject(debugDataJSON, "ap", cJSON_CreateNumber(acu.AIRPlusIsArmed));
			 	 		 cJSON_AddItemToObject(debugDataJSON, "am", cJSON_CreateNumber(acu.AIRMinusIsArmed));
			 	 		 cJSON_AddItemToObject(debugDataJSON, "p", cJSON_CreateNumber( acu.PREIsArmed));
			 	 		 cJSON_AddItemToObject(debugDataJSON, "ts", cJSON_CreateNumber(acu.TSOver60Volt));
			 	 		 cJSON_AddItemToObject(debugDataJSON, "i", cJSON_CreateNumber(acu.IMDHasError));
			 	 		 cJSON_AddItemToObject(debugDataJSON, "a", cJSON_CreateNumber(acu.AMSHasError));
			 	 		 cJSON_AddItemToObject(debugDataJSON, "st", cJSON_CreateNumber(acu.AIRsAreStuck));
			 	 		cJSON_AddItemToObject(debugDataJSON, "c", cJSON_CreateNumber(hfdcan1.ErrorCode));
			 	 		cJSON_AddItemToObject(debugDataJSON, "f", cJSON_CreateNumber(flag));


			 	 		cJSON *devDataJSON = cJSON_CreateObject();
			 	 		cJSON_AddItemToObject(root, "dev", devDataJSON);

			 	 		 cJSON_AddItemToObject(devDataJSON, "acu", cJSON_CreateNumber(acu.commFlag));
			 	 		 cJSON_AddItemToObject(devDataJSON, "ivt", cJSON_CreateNumber(ivt.isConnected));
			 	 		 cJSON_AddItemToObject(devDataJSON, "inv", cJSON_CreateNumber(inverter.isConnected));
			 	 		 cJSON_AddItemToObject(devDataJSON, "ddl", cJSON_CreateNumber(/*dataLogger.mainIsConnected*/jsonCounter++));
			 	 		 cJSON_AddItemToObject(devDataJSON, "cbdl", cJSON_CreateNumber(dataLogger.controlBoxIsConnected));
			 	 		 cJSON_AddItemToObject(devDataJSON, "ae", cJSON_CreateNumber(dataLogger.aeroIsConnected));
			 	 		 cJSON_AddItemToObject(devDataJSON, "tel", cJSON_CreateNumber(vcu.telemetryIsConnected));
			 	 		 cJSON_AddItemToObject(devDataJSON, "usr", cJSON_CreateNumber(acu.userIsConnected));

			 	 // SD
			 	 		 cJSON *ecuDataJSON = cJSON_CreateObject();
			 	 		 cJSON_AddItemToObject(root, "ecu", ecuDataJSON);

			 	 		 cJSON_AddItemToObject(ecuDataJSON, "a", cJSON_CreateNumber(vcu.pedalSensorFinalValue));
			 	 		 cJSON_AddItemToObject(ecuDataJSON, "b", cJSON_CreateNumber((int16_t)(vcu.brakePressure * 100)));
			 	 		 cJSON_AddItemToObject(ecuDataJSON, "mc", cJSON_CreateNumber((uint8_t)map(inverter.motorTemp, 10, 60, 20, 100)));
			 	 		 cJSON_AddItemToObject(ecuDataJSON, "ic", cJSON_CreateNumber((uint8_t)map(inverter.igbtTemp, 10, 60, 20, 100)));
			 	 		 cJSON_AddItemToObject(ecuDataJSON, "f", cJSON_CreateNumber(vcu.state));

				 	 	cJSON *cbdlDataJSON = cJSON_CreateObject();
				 	 	cJSON_AddItemToObject(root, "cbdl", cbdlDataJSON);

				 	 	cJSON_AddItemToObject(cbdlDataJSON, "ax", cJSON_CreateNumber(dataLogger.Ax));
				 	 	cJSON_AddItemToObject(cbdlDataJSON, "ay", cJSON_CreateNumber(dataLogger.Ay));
				 	 	cJSON_AddItemToObject(cbdlDataJSON, "az", cJSON_CreateNumber(dataLogger.Az));
				 	 	cJSON_AddItemToObject(cbdlDataJSON, "gx", cJSON_CreateNumber(dataLogger.Gx));
				 	 	cJSON_AddItemToObject(cbdlDataJSON, "gy", cJSON_CreateNumber(dataLogger.Gy));
				 	 	cJSON_AddItemToObject(cbdlDataJSON, "gz", cJSON_CreateNumber(dataLogger.Gz));
				 	    cJSON_AddItemToObject(cbdlDataJSON, "rr", cJSON_CreateNumber(dataLogger.rearRightWheelRPM));
				 	 	cJSON_AddItemToObject(cbdlDataJSON, "rl", cJSON_CreateNumber(dataLogger.rearLeftWheelRPM));

				 	 	cJSON *acuDataJSON = cJSON_CreateObject();
				 	 	cJSON_AddItemToObject(root, "acu", acuDataJSON);

				 	 	cJSON_AddItemToObject(acuDataJSON, "f", cJSON_CreateNumber(acu.commFlag));
				 	 	cJSON_AddItemToObject(acuDataJSON, "imd_r", cJSON_CreateNumber(acu.IMDResistance));
				 	 	cJSON_AddItemToObject(acuDataJSON, "t", cJSON_CreateNumber(acu.PCBTemperature));
				 	 	cJSON_AddItemToObject(acuDataJSON, "h", cJSON_CreateNumber(1 /*humidity*/));
				 	 	cJSON_AddItemToObject(acuDataJSON, "vt", cJSON_CreateNumber(acu.vicorTemperature));
				 	 	cJSON_AddItemToObject(acuDataJSON, "maxv_v", cJSON_CreateNumber((int8_t)acu.maxVoltageCell.val));
				 	    cJSON_AddItemToObject(acuDataJSON, "maxv_i", cJSON_CreateNumber(acu.maxVoltageCell.id));
				 	 	cJSON_AddItemToObject(acuDataJSON, "minv_v", cJSON_CreateNumber((int8_t)acu.minVoltageCell.val));
				 	 	cJSON_AddItemToObject(acuDataJSON, "minv_i", cJSON_CreateNumber(acu.minVoltageCell.id));
				 	 	cJSON_AddItemToObject(acuDataJSON, "maxt_v", cJSON_CreateNumber((int8_t)acu.maxCellTemp.val));
				 	 	cJSON_AddItemToObject(acuDataJSON, "maxt_i", cJSON_CreateNumber(acu.maxCellTemp.id));

				 	 	cJSON *ivtDataJSON = cJSON_CreateObject();
				 	 	cJSON_AddItemToObject(root, "ivt", ivtDataJSON);

				 	    cJSON_AddItemToObject(ivtDataJSON, "c", cJSON_CreateNumber(ivt.current));
				 	 	cJSON_AddItemToObject(ivtDataJSON, "cc", cJSON_CreateNumber(ivt.currentCounter));
				 	 	cJSON_AddItemToObject(ivtDataJSON, "w", cJSON_CreateNumber(ivt.wattage));
				 	 	cJSON_AddItemToObject(ivtDataJSON, "wc", cJSON_CreateNumber(ivt.wattageCounter));
				 	 	cJSON_AddItemToObject(ivtDataJSON, "v", cJSON_CreateNumber(ivt.voltage1));
				 	 	cJSON_AddItemToObject(ivtDataJSON, "b", cJSON_CreateNumber(ivt.voltage2));

				 	 	cJSON *invDataJSON = cJSON_CreateObject();
				 	 	cJSON_AddItemToObject(root, "inv", invDataJSON);

				 	    cJSON_AddItemToObject(invDataJSON, "mr", cJSON_CreateNumber(inverter.RPMSpeed));
				 	 	cJSON_AddItemToObject(invDataJSON, "mt", cJSON_CreateNumber(inverter.motorTemp));
				 	 	cJSON_AddItemToObject(invDataJSON, "it", cJSON_CreateNumber(inverter.igbtTemp));
				 	 	cJSON_AddItemToObject(invDataJSON, "c", cJSON_CreateNumber(inverter.actualCurrent));
				 	 	cJSON_AddItemToObject(invDataJSON, "cq", cJSON_CreateNumber(inverter.actualCurrentQ));
				 	 	cJSON_AddItemToObject(invDataJSON, "t", cJSON_CreateNumber(inverter.torqueCommand));


		 	 				}
				char *ret = cJSON_PrintUnformatted(root);
				cJSON_Delete(root);
				snprintf(&response_template[strlen(response_template)], sizeof(response_template) - strlen(response_template),
	             "Content-Length: %" PRIu32 "\r\n\r\n%s", (uint32_t)strlen(ret), ret);

	    response_data = response_template;
	    vPortFree(ret);

//
//					cJSON *ivtJSON = cJSON_CreateArray();
//					cJSON_AddItemToObject(root, "ivt", ivtJSON);
//					cJSON *ivtDatasJSON = cJSON_CreateObject();
//					cJSON_AddItemToArray(ivtJSON, ivtDatasJSON);
//
//					cJSON_AddItemToObject(ivtDatasJSON, "i", cJSON_CreateNumber(ivt.hasError));
//					cJSON_AddItemToObject(ivtDatasJSON, "c", cJSON_CreateNumber((int32_t)(1000*ivt.current)));
//					cJSON_AddItemToObject(ivtDatasJSON, "cc", cJSON_CreateNumber(ivt.currentCounter));
//					cJSON_AddItemToObject(ivtDatasJSON, "v", cJSON_CreateNumber((int32_t)(1000*ivt.voltage1)));
//					cJSON_AddItemToObject(ivtDatasJSON, "w", cJSON_CreateNumber((int32_t)(ivt.wattage)));
//					cJSON_AddItemToObject(ivtDatasJSON, "wc", cJSON_CreateNumber(ivt.wattageCounter));
//
//					cJSON *chargerJSON = cJSON_CreateArray();
//					cJSON_AddItemToObject(root, "charger", chargerJSON);
//					cJSON *chargerDatasJSON = cJSON_CreateObject();
//					cJSON_AddItemToArray(chargerJSON, chargerDatasJSON);
//
//					cJSON_AddItemToObject(chargerDatasJSON, "c", cJSON_CreateNumber((int16_t)(10*charger.currentAtCharger)));
//					cJSON_AddItemToObject(chargerDatasJSON, "v", cJSON_CreateNumber((int16_t)(10*charger.voltageAtCharger)));
//					cJSON_AddItemToObject(chargerDatasJSON, "e", cJSON_CreateNumber((charger.statusFlag << 1) | charger.isConnected));
//
//					cJSON *acuJSON = cJSON_CreateArray();
//					uint16_t acuFlags = acu.airPlusAux.state | acu.airPlusSupply.state << 1 |
//							acu.airMinusAux.state << 2| acu.preAux.state << 3 | acu.airMinusSupply.state << 4 |
//							acu.TSOver60Volt.state << 5 | acu.IMDLatchedError.state << 6 | bms.latchedError.state << 7;
//					uint8_t bmsFlags = bms.hasError | bms.latchedError.state << 1 | bms.voltagesHaveError << 2 | bms.tempsHaveError << 3 |
//							ivt.hasError << 4 | bms.isoSPIHasError << 5 | bms.internalVoltagesHaveError << 6;
//
//					cJSON_AddItemToObject(root, "acu", acuJSON);
//					cJSON *acuDatasJSON = cJSON_CreateObject();
//					cJSON_AddItemToArray(acuJSON, acuDatasJSON);
//					cJSON_AddItemToObject(acuDatasJSON, "f", cJSON_CreateNumber(acuFlags));
//					cJSON_AddItemToObject(acuDatasJSON, "t", cJSON_CreateNumber((int8_t)acu.PCBTemperature));
//					cJSON_AddItemToObject(acuDatasJSON, "h", cJSON_CreateNumber((int8_t)acu.humidity));
//					cJSON_AddItemToObject(acuDatasJSON, "bf", cJSON_CreateNumber(bmsFlags));
//					cJSON_AddItemToObject(acuDatasJSON, "imd_f", cJSON_CreateNumber((int8_t)acu.IMDPWMFrequency));
//					cJSON_AddItemToObject(acuDatasJSON, "imd_r", cJSON_CreateNumber(acu.IMDResistance));
//					cJSON_AddItemToObject(acuDatasJSON, "q", cJSON_CreateNumber(currentResponse%1000));
//					cJSON_AddItemToObject(acuDatasJSON, "e", cJSON_CreateNumber(errorHandlerFlag));
//					cJSON_AddItemToObject(acuDatasJSON, "c1", cJSON_CreateNumber(hfdcan1.ErrorCode));
//					cJSON_AddItemToObject(acuDatasJSON, "c2", cJSON_CreateNumber(hfdcan2.ErrorCode));
//					cJSON_AddItemToObject(acuDatasJSON, "s", cJSON_CreateNumber(hspi1.ErrorCode));
//					cJSON_AddItemToObject(acuDatasJSON, "sd", cJSON_CreateNumber(hsd1.ErrorCode));
//
//
//
//					cJSON *vicorJSON = cJSON_CreateArray();
//					cJSON_AddItemToObject(root, "vicor", vicorJSON);
//					cJSON *vicorDatasJSON = cJSON_CreateObject();
//					cJSON_AddItemToArray(vicorJSON, vicorDatasJSON);
//
////					vicorCurrentIn = currentResponse %256;
////
////					vicorCurrentOut = currentResponse %256;
////					vicorVoltageIn = currentResponse %256;
////					vicorVoltageOut = currentResponse %256;
////					vicorTemperature = currentResponse %256;
//
//					cJSON_AddItemToObject(vicorDatasJSON, "v_in", cJSON_CreateNumber((uint16_t)vicor.voltageIn*10));
//					cJSON_AddItemToObject(vicorDatasJSON, "v_out", cJSON_CreateNumber((uint16_t)vicor.voltageOut*10));
//					cJSON_AddItemToObject(vicorDatasJSON, "i_in", cJSON_CreateNumber((uint16_t)(vicor.currentIn*1000)));
//					cJSON_AddItemToObject(vicorDatasJSON, "i_out", cJSON_CreateNumber((uint16_t)(vicor.currentOut*100)));
//					cJSON_AddItemToObject(vicorDatasJSON, "t", cJSON_CreateNumber(vicor.temperature));
//					cJSON_AddItemToObject(vicorDatasJSON, "w", cJSON_CreateNumber((uint16_t)vicor.wattageOut));
//					cJSON_AddItemToObject(vicorDatasJSON, "e", cJSON_CreateNumber(vicor.i2cErrorCode));
//					cJSON_AddItemToObject(vicorDatasJSON, "i", cJSON_CreateNumber(hi2c1.ErrorCode));
//
//				}
//				char *ret = cJSON_PrintUnformatted(root);
//				cJSON_Delete(root);
//				snprintf(&response_template[strlen(response_template)], sizeof(response_template) - strlen(response_template),
//	             "Content-Length: %" PRIu32 "\r\n\r\n%s", (uint32_t)strlen(ret), ret);
//	    currentResponse++;
//
//	    response_data = response_template;
//	    vPortFree(ret);

	    if (1 == http_server_write(client, response_data, strlen(response_data)))
	    {
	      return;
	    }

  }

//  else if(response == CHARGE_COMMAND){
//	  chargePost++;
//	  static const char HTTP_RESPONSE_CONTINUE[] =
//	    "HTTP/1.1 100 Continue\r\n\r\n";
//	  static const char HTTP_RESPONSE_OK[] =
//	    "HTTP/1.1 200 OK\r\n"
//	    "Server: U5\r\n"
//	    "Access-Control-Allow-Origin: *\r\n"
//	    "Cache-Control: no-cache\r\n"
//	    "Connection: close\r\n"
//	    "Content-Type: text/plain; charset=utf-8\r\n"
//	    "Content-Length: 2\r\n"
//	    "\r\n"
//	    "OK";
//	    if (1 == http_server_write(client, HTTP_RESPONSE_CONTINUE, strlen(HTTP_RESPONSE_CONTINUE)))
//	    {
//	      return;
//	    }
//
//	    size_t recv_buffer_len = 200;
//	    char *recv_buffer = (char *)pvPortMalloc(recv_buffer_len);
//	    int32_t bytes_received;
//	    int32_t recv_total_len = 0;
//	    do
//	    {
//	      bytes_received = W6X_Net_Recv(client, (uint8_t *)&recv_buffer[recv_total_len], recv_buffer_len, 0);
//	      if (bytes_received < 0) /* No data received or error */
//	      {
//	        break;
//	      }
//
//	      /* Case where we have receive less than the buffer allocated */
//	      if (bytes_received < recv_buffer_len)
//	      {
//	        recv_total_len += bytes_received;
//	        break;
//	      }
//
//	      recv_total_len += bytes_received;
//	      recv_buffer_len -= bytes_received;
//	    } while ((bytes_received != 0) && (recv_buffer_len > 0));
//	    if (1 == http_server_write(client, HTTP_RESPONSE_OK, strlen(HTTP_RESPONSE_OK)))
//	    {
//	      return;
//	    }
//
//	    cJSON *root = NULL;
//	    cJSON *currentInput = NULL;
//	    cJSON *chargingStateInput = NULL;
//	    cJSON *passwordInput = NULL;
//	    cJSON *balancingStateInput = NULL;
//	    cJSON_Hooks hooks =
//	    {
//	      .malloc_fn = pvPortMalloc,
//	      .free_fn = vPortFree,
//	    };
//	    cJSON_InitHooks(&hooks);
//	    char *json_start = strstr(recv_buffer, "{");
//
//	    root = cJSON_Parse((const char *)json_start);
//
//	    passwordInput = cJSON_GetObjectItemCaseSensitive(root, "pass");
//	    currentInput = cJSON_GetObjectItemCaseSensitive(root, "current");
//	    chargingStateInput = cJSON_GetObjectItemCaseSensitive(root, "cs");
//	    balancingStateInput = cJSON_GetObjectItemCaseSensitive(root, "bs");
//	    vPortFree(recv_buffer);//Xreiazetai
//
//	    if(passwordInput->valueint == 69420){
//
////	    	charger.wantedCurrentTimes10 = currentInput->valueint;
////	    	charger.wantedOff = !(chargingStateInput->valueint);
////	    	bms.cellBalancingIsWanted = balancingStateInput->valueint;
//	    //	HAL_GPIO_TogglePin(LED_CAN_1_GPIO_Port, LED_CAN_1_Pin);
//
//	    }
//	    cJSON_Delete(root);
//
//  }

}

/* USER CODE END PFD */
