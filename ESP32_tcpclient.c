#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "addr_from_stdin.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"

//DHT111
#include "driver/gpio.h"
#include "sdkconfig.h"

//TEMT6000
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define EXAMPLE_ESP_WIFI_SSID      "wangguangjie"
#define EXAMPLE_ESP_WIFI_PASS      "123454321"

#define HOST_IP_ADDR "192.168.1.111"


#define PORT 8888

static const char *TAG = "tcp_client";
static const char *payload = "Message from ESP32C3 ";

uint8_t connect_count = 0;


/*Read DHT11 temp & Humi*/
#define DHT11_PIN     (21)   //可通过宏定义，修改引脚

#define DHT11_CLR     gpio_set_level(DHT11_PIN, 0) 
#define DHT11_SET     gpio_set_level(DHT11_PIN, 1) 
#define DHT11_IN      gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT)
#define DHT11_OUT     gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT)

uint8_t DHT11Data[4]={0};
uint8_t Temp, Humi;

//us延时函数，误差不能太大
void DelayUs(  uint32_t nCount)  
{
    ets_delay_us(nCount);
}  

void DHT11_Start(void)
{ 
    DHT11_OUT;      //设置端口方向
    DHT11_CLR;      //拉低端口  
    DelayUs(19*1000);   
    //   vTaskDelay(19 * portTICK_RATE_MS); //持续最低18ms;

    DHT11_SET;      //释放总线
    DelayUs(30);    //总线由上拉电阻拉高，主机延时30uS;
    DHT11_IN;       //设置端口方向

    while(!gpio_get_level(DHT11_PIN));   //DHT11等待80us低电平响应信号结束
    while(gpio_get_level(DHT11_PIN));//DHT11   将总线拉高80us
}

uint8_t DHT11_ReadValue(void)
{
    uint8_t i,sbuf=0;
    for(i=8;i>0;i--)
    {
        sbuf<<=1;
        while(!gpio_get_level(DHT11_PIN));
        DelayUs(30);                        // 延时 30us 后检测数据线是否还是高电平 
        if(gpio_get_level(DHT11_PIN))
        {
            sbuf|=1;  
        }
        else
        {
            sbuf|=0;
        }
        while(gpio_get_level(DHT11_PIN));
    }
    
    return sbuf;
}

uint8_t DHT11_ReadTemHum(uint8_t *buf)
{
    uint8_t check;

    buf[0]=DHT11_ReadValue();
    buf[1]=DHT11_ReadValue();
    buf[2]=DHT11_ReadValue();
    buf[3]=DHT11_ReadValue();

    check =DHT11_ReadValue();

    if(check == buf[0]+buf[1]+buf[2]+buf[3])
        return 1;
    else
        return 0;
}
/*add for DHT111 end*/

/*add for TEMT6000*/
// ADC所接的通道  GPIO34 if ADC1  = ADC1_CHANNEL_6
#define ADC1_TEST_CHANNEL ADC1_CHANNEL_6 
// ADC斜率曲线
static esp_adc_cal_characteristics_t *adc_chars;
// 参考电压
#define DEFAULT_VREF				3300			//使用adc2_vref_to_gpio（）获得更好的估计值

void check_efuse(void)
{
    //检查TP是否烧入eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //检查Vref是否烧入eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}
void adc_init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);// 12位分辨率
	adc1_config_channel_atten(ADC1_TEST_CHANNEL, ADC_ATTEN_DB_11);// 电压输入衰减
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));	// 为斜率曲线分配内存
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
}

void readLight(int *LightValue)
{
    int read_raw = adc1_get_raw(ADC1_TEST_CHANNEL);// 采集ADC原始值//这里可以多次采样取平均值
    uint32_t voltage = esp_adc_cal_raw_to_voltage(read_raw, adc_chars);//通过一条斜率曲线把读取adc1_get_raw()的原始数值转变成了mV
    printf("ADC原始值: %d   转换电压值: %dmV\n", read_raw, voltage);
    *LightValue = read_raw;
}
/*add TEMP6000 END*/

/*add GPIO start*/
#define LED_PIN 2       //LED
#define BUZZER_PIN 22    //蜂鸣器
#define SWITCH1_PIN 18  
#define SWITCH2_PIN 5

void setLed(int LedFlag)
{
    printf("The func: %s line: %d \n", __FUNCTION__, __LINE__);
    if(1 == LedFlag)
    {
        gpio_set_level(LED_PIN,1);                    //设置LED_PIN为高电平
    }else if(0 == LedFlag)
    {
        gpio_set_level(LED_PIN,0);                    //设置LED_PIN为低电平
    }
}

void setBuzzer(int ringFlag)
{
    printf("The func: %s line: %d \n", __FUNCTION__, __LINE__);
    if(1 == ringFlag)
    {
        gpio_set_level(BUZZER_PIN,1);                    //设置BUZZER_PIN为高电平
    }else if(0 == ringFlag)
    {
        gpio_set_level(BUZZER_PIN,0);                    //设置BUZZER_PIN为低电平
    }
}

void setSwitch1(int switch1Flag)
{
    //AC
    printf("The func: %s line: %d \n", __FUNCTION__, __LINE__);
    if(1 == switch1Flag)
    {
        gpio_set_level(SWITCH1_PIN,1);                    //设置SWITCH1_PIN为高电平
    }else if(0 == switch1Flag)
    {
        gpio_set_level(SWITCH1_PIN,0);                    //设置SWITCH1_PIN为低电平
    }
}

void setSwitch2(int switch2Flag)
{
    //FAN
    printf("The func: %s line: %d \n", __FUNCTION__, __LINE__);
    if(1 == switch2Flag)
    {
        gpio_set_level(SWITCH2_PIN,1);                    //设置SWITCH1_PIN为高电平
    }else if(0 == switch2Flag)
    {
        gpio_set_level(SWITCH2_PIN,0);                    //设置SWITCH2_PIN为低电平
    }
}
/*add GPIO end*/

void ctlDevbyServer(char *devType, int switchFlag)
{
    // 将输入字符串转换为小写
    char str[30] = {0};
    memcpy(str, devType, sizeof(str));
    for(int i = 0; str[i]; i++){
        str[i] = tolower(str[i]);
    }

    // 使用 switch case 解析字符串
    switch (str[0]) {
        case 'l':
            if (strcmp(str, "led") == 0) {
                printf("执行 LED 操作\n");
                setLed(switchFlag);
            } else if (strcmp(str, "led_buzzer") == 0) {
                printf("执行 LED_BUZZER 操作\n");
                setLed(switchFlag);
                setBuzzer(switchFlag);
            } else {
                printf("未知操作\n");
            }
            break;
        case 'b':
            if (strcmp(str, "buzzer") == 0) {
                printf("执行 BUZZER 操作\n");
                setBuzzer(switchFlag);
            } else {
                printf("未知操作\n");
            }
            break;
        case 'a':
            if (strcmp(str, "ac") == 0) {
                printf("执行 AC 操作\n");
                setSwitch1(switchFlag);
            } else if (strcmp(str, "ac_fan") == 0) {
                printf("执行 AC_FAN 操作\n");
                setSwitch1(switchFlag);
                setSwitch2(switchFlag);
            } else {
                printf("未知操作\n");
            }
            break;
        case 'f':
            if (strcmp(str, "fan") == 0) {
                printf("执行 FAN 操作\n");
                setSwitch2(switchFlag);
            } else {
                printf("未知操作\n");
            }
            break;
         case 'r':
            if (strcmp(str, "ring") == 0) {
                printf("执行 RING 操作\n");
                setBuzzer(switchFlag);
            } else {
                printf("未知操作\n");
            }
            break;
        default:
            printf("未知操作\n");
    }
}

void parseString(char *str) {
    char *token = strtok(str, " ");

    while (token != NULL) {
        // 寻找_ON 或 _OFF的位置
        char *index_on = strstr(token, "_ON");
        char *index_off = strstr(token, "_OFF");

        if (index_on != NULL) {
            *index_on = '\0';  // 截断字符串
            printf("%s\n", token);
            ctlDevbyServer(token, 1);
        } else if (index_off != NULL) {
            *index_off = '\0'; // 截断字符串
            printf("%s\n", token);
            ctlDevbyServer(token, 0);
        }

        token = strtok(NULL, " ");
    }
}

static void tcp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;


    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(host_ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    //init DHT11
    char sendStr[128] = {0};

    //init dev
    gpio_reset_pin(LED_PIN);                         //引脚复位
    gpio_pad_select_gpio(LED_PIN);                   //GPIO引脚功能选择
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);   //设置方向为输出

    gpio_reset_pin(BUZZER_PIN);                         //引脚复位
    gpio_pad_select_gpio(BUZZER_PIN);                   //GPIO引脚功能选择
    gpio_set_direction(BUZZER_PIN, GPIO_MODE_OUTPUT);   //设置方向为输出

    gpio_reset_pin(SWITCH1_PIN);                         //引脚复位
    gpio_pad_select_gpio(SWITCH1_PIN);                   //GPIO引脚功能选择
    gpio_set_direction(SWITCH1_PIN, GPIO_MODE_OUTPUT);   //设置方向为输出

    gpio_reset_pin(SWITCH2_PIN);                         //引脚复位
    gpio_pad_select_gpio(SWITCH2_PIN);                   //GPIO引脚功能选择
    gpio_set_direction(SWITCH2_PIN, GPIO_MODE_OUTPUT);   //设置方向为输出

    // //init TEMP6000
    check_efuse();
	adc_init();

    int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
    if (err != 0) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
    }
    ESP_LOGI(TAG, "Successfully connected");

    int temp= 0 , humi = 0;     //temp & humi
    int lightValue = 0, smoke = 55;
    int index = 0;
    while (1) {
        //read sensor data
        if((index % 5 == 0))
        {
            readLight(&lightValue);
            if(index >= 15)
            {
                index = 1;
                DHT11_Start();
                if(DHT11_ReadTemHum(DHT11Data))
                {
                    temp=DHT11Data[2];
                    humi=DHT11Data[0];      
                    printf("Temp=%d, Humi=%d\r\n",temp,humi);
                }
                else
                {
                    printf("Read temp & humi failed !\n");
                }
            }
        }

        sprintf(sendStr, "TEMP: %d HUMI: %d LIGHT: %d SMOKE: %d", temp, humi, lightValue, smoke);
        int err = send(sock, sendStr, strlen(sendStr), 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
        }

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, MSG_DONTWAIT);
        // Error occurred during receiving
        if (len > 0){
            rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
            ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
            ESP_LOGI(TAG, "%s", rx_buffer);
            parseString(rx_buffer);
        }

        index++;
        vTaskDelay(2000 / portTICK_PERIOD_MS);

       
    }

    if (sock != -1) {
        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }

    vTaskDelete(NULL);
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    //WIFI启动成功
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    //WIFI连接失败
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        connect_count++;
        if(connect_count<=5)
        {
            esp_wifi_connect();
        }
        else
        {
            printf("WIFI连接失败\n");
        }
    }
    //STA成功介入热点并且获取到了IP
    if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        printf("STA成功接入热点\n");
        ip_event_got_ip_t* info = (ip_event_got_ip_t*)event_data;
        printf("STA的IP是"IPSTR"\n",IP2STR(&info->ip_info.ip));
        
        gpio_pad_select_gpio(DHT11_PIN);
        xTaskCreate(tcp_client_task, "tcp_client", 8192, NULL, 5, NULL);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_STA_START,wifi_event_handler,NULL,NULL);
    esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,wifi_event_handler,NULL,NULL);
    esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event_handler,NULL,NULL);

    //初始化以下网络接口
    esp_netif_init();

    //创建一个STA类型的网卡
    esp_netif_create_default_wifi_sta();

    //初始化WIFI的底层配置
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_cfg);

    //设置WIFI的模式
    esp_wifi_set_mode(WIFI_MODE_STA);

    //配置STA的相关参数
    wifi_config_t sta_cfg = 
    {
        .sta =
        {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        }
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA,&sta_cfg);
    esp_wifi_start();
    //esp_wifi_connect();
    esp_wifi_set_ps(WIFI_PS_NONE);//不需要省电！
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    wifi_init_softap();
}
