/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ian Craggs - initial API and implementation and/or initial documentation
 *    Sergio R. Caprile - "commonalization" from prior samples and/or documentation extension
 *******************************************************************************/




#include "transport.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "usart.h"
#include "MQTTPacket.h"
#include "timer.h"
#include "gprs.h"



extern usart_buff_t usart1_rx_buff;
extern usart_buff_t usart2_rx_buff;
extern usart_buff_t mqtt_buff;
extern u8 send_buff[100];


u8 subscribe_status = PUBLISH;

int mqtt_buff_len = 0;

u32 packet_id = 0;
uint16_t mqtt_publist_packetid = 0;				//发布消息id
uint16_t mqtt_subscribe_packetid = 0;			//订阅消息id

/**
This simple low-level implementation assumes a single connection for a single thread. Thus, a static
variable is used for that connection.
On other scenarios, the user must solve this by taking into account that the current implementation of
MQTTPacket_read() has a function pointer for a function call to get the data to a buffer, but no provisions
to know the caller or other indicator (the socket id): int (*getfn)(unsigned char*, int)
*/
//static int mysock = INVALID_SOCKET;


int transport_sendPacketBuffer(int sock, unsigned char* buf, int buflen)
{
	int rc = 0;
	u8 *ret;
	int len = 0;
	u8 cmd[30] = {0};
//	u8 end_char[2] = {0};

//	end_char[0] = 0x1A;//结束字符
//	end_char[1] = '\0';
	
	memset(send_buff, 0, sizeof(send_buff));
	memset(&mqtt_buff, 0, sizeof(usart_buff_t));
	mqtt_buff_len = 0;
	sprintf((char *)cmd, "AT+CIPSEND=%d,1\r\n", buflen);
	ret = gprs_send_at(cmd, ">", 20, 100);
	if(ret != NULL)
	{
		memcpy((char *)send_buff, buf, buflen);
//		memcpy((char *)send_buff+buflen, (char*)end_char, sizeof(end_char));
		usart_send(USART2, send_buff, buflen);	
//		gprs_send_data(send_buff, buflen+3, "SEND OK", 100);
	}
	
	rc = buflen+3;
	return rc;
}


int transport_getdata(unsigned char* buf, int count)
{
	if(mqtt_buff.index > 0)
	{
		memcpy(buf, &mqtt_buff.pdata[mqtt_buff_len], count);
//		usart_send(USART1, mqtt_buff->pdata, mqtt_buff->index);
//		usart_send(USART1, &mqtt_buff->pdata[mqtt_buff_len], count);
		mqtt_buff_len += count;
		return count;
	}
	
}



int toStop = 0;



void mqtt_qos0(void)
{
	
	
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	int mysock = 0;
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int msgid = 1;
	MQTTString topicString = MQTTString_initializer;
	int req_qos = 0;
	char* payload = "mypayload";
	int payloadlen = strlen(payload);
	int len = 0;
	char *host = "m2m.eclipse.org";
	int port = 1883;


	data.clientID.cstring = "me";
	data.keepAliveInterval = 60;
	data.cleansession = 1;
	data.username.cstring = "testuser";
	data.password.cstring = "testpassword";

	len = MQTTSerialize_connect(buf, buflen, &data);
		
	usart_send(USART1, buf, len);
	
	rc = transport_sendPacketBuffer(mysock, buf, len);

	/* wait for connack */
	if (MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
	{
		unsigned char sessionPresent, connack_rc;

		if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
		{
			USART_OUT(USART1, "Unable to connect, return code %d\n", connack_rc);
		}
	}
	
		

	/* subscribe */
	topicString.cstring = "substopic";
	len = MQTTSerialize_subscribe(buf, buflen, 0, msgid, 1, &topicString, &req_qos);

	rc = transport_sendPacketBuffer(mysock, buf, len);
	if (MQTTPacket_read(buf, buflen, transport_getdata) == SUBACK) 	/* wait for suback */
	{
		unsigned short submsgid;
		int subcount;
		int granted_qos;

		rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
		if (granted_qos != 0)
		{
			USART_OUT(USART1, "granted qos != 0, %d\n", granted_qos);
		}
	}
	

	/* loop getting msgs on subscribed topic */
	topicString.cstring = "pubtopic";
	while (!toStop)
	{
		/* transport_getdata() has a built-in 1 second timeout,
		your mileage will vary */
		if (MQTTPacket_read(buf, buflen, transport_getdata) == PUBLISH)
		{
			unsigned char dup;
			int qos;
			unsigned char retained;
			unsigned short msgid;
			int payloadlen_in;
			unsigned char* payload_in;
			int rc;
			MQTTString receivedTopic;

			rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,
					&payload_in, &payloadlen_in, buf, buflen);

			USART_OUT(USART1, "message arrived %.*s\n", payloadlen_in, payload_in);
		}


		USART_OUT(USART1, "publishing reading\n");
		len = MQTTSerialize_publish(buf, buflen, 0, 0, 0, 0, topicString, (unsigned char*)payload, payloadlen);
		rc = transport_sendPacketBuffer(mysock, buf, len);
	}

	USART_OUT(USART1, "disconnecting\n");
	len = MQTTSerialize_disconnect(buf, buflen);
	rc = transport_sendPacketBuffer(mysock, buf, len);
}






int mqtt_connect(void)
{
	int ret = 0;
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	int mysock = 0;
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int len = 0;
//	char *host = "118.31.69.148";
//	int port = 1883;

	data.clientID.cstring = "me1";
	data.keepAliveInterval = 60;
	data.cleansession = 1;
//	data.username.cstring = "";
//	data.password.cstring = "";

	len = MQTTSerialize_connect(buf, buflen, &data);		
	rc = transport_sendPacketBuffer(mysock, buf, len);
	
	timer_is_timeout_1ms(timer_connect, 0);
	while(!ret)
	{ 
		usart2_recv_data();

		if(timer_is_timeout_1ms(timer_connect, 2000) == 0)
		{
			len = MQTTSerialize_connect(buf, buflen, &data);		
			rc = transport_sendPacketBuffer(mysock, buf, len);
		}
		
		if (MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
		{
			unsigned char sessionPresent, connack_rc;

			if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
			{
				USART_OUT(USART1, "Unable to connect, return code %d\n", connack_rc);
			}
			else
			{
				ret = 1;
				break;
			}
		}
		
		
	}
	
	return ret;
}




int mqtt_publist(unsigned char* topic, unsigned char* payload, int payload_len, int qos, unsigned short packetid)
{
	u8 ret = 0;
	int rc = 0;
	int len = 0;
	char buf[200];
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTString topicString = MQTTString_initializer;
	u8 publist_status = PUBLISH;
	
	while(!ret)
	{
		usart2_recv_data();
		switch(publist_status)
		{
			case PUBLISH:
				topicString.cstring = topic;				
//				strcpy(topicString.cstring, "test");
				len = MQTTSerialize_publish((unsigned char *)buf , buflen, 0, qos, 0, packetid, topicString, (unsigned char *)payload, payload_len);
				rc = transport_sendPacketBuffer(mysock, buf, len);
				publist_status = PUBREC;
				
			break;
				
			case PUBREC:
				if (MQTTPacket_read(buf, buflen, transport_getdata) == PUBREC)
				{
					publist_status = PUBREL;
				}	
			break;
				
			case PUBREL:
				len = MQTTSerialize_pubrel(buf, buflen, 0, packetid);
				rc = transport_sendPacketBuffer(mysock, buf, len);
				publist_status = PUBCOMP;
			break;

			case PUBCOMP:
				if (MQTTPacket_read(buf, buflen, transport_getdata) == PUBCOMP)
				{
					ret = 1;
				}	
			break;
				
			default:
			break;	
		}	
	}
	
	return ret;
}

void mqtt_subscribe(unsigned char* topic, unsigned short packetid)
{
	
	u8 ret = 0;
	int rc = 0;
	int len = 0;
	char buf[200];
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTString topicString = MQTTString_initializer;
	u8 publist_status = PUBLISH;
	

	switch(subscribe_status)
	{
		case PUBLISH:			
			if (MQTTPacket_read(buf, buflen, transport_getdata) == PUBLISH)
			{
				unsigned char dup;
				int qos;
				unsigned char retained;
				unsigned short msgid;
				int payloadlen_in;
				unsigned char* payload_in;
				int rc;
				MQTTString receivedTopic;
				
				rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,
				&payload_in, &payloadlen_in, buf, buflen);
				
				USART_OUT(USART1, "message arrived %.*s\n", payloadlen_in, payload_in);
				subscribe_status = PUBREC;
			}
		break;
			
		case PUBREC:
			len = MQTTSerialize_puback(buf, buflen, packetid);
			rc = transport_sendPacketBuffer(mysock, buf, len);
		
			subscribe_status = PUBREL;			
		break;
			
		case PUBREL:
			if (MQTTPacket_read(buf, buflen, transport_getdata) == PUBREL)
			{
				subscribe_status = PUBCOMP;
			}				
		break;

		case PUBCOMP:
			len = MQTTSerialize_pubrel(buf, buflen, 0, packetid);
			rc = transport_sendPacketBuffer(mysock, buf, len);
			subscribe_status = PUBLISH;
		break;
			
		default:
		break;	
	}	
	
	
}



int mqtt_subscribe_msg(unsigned char* topic, int req_qos, unsigned short packetid)
{
	int ret = 0;
	unsigned char buf[200];
	int buflen = sizeof(buf);
	int len = 0;
	int rc = 0;
	int mysock = 0;
	u8 subscribe_status = SUBSCRIBE;
	MQTTString topicString = MQTTString_initializer;
	
	
	while(!ret)
	{
		usart2_recv_data();
		
		switch(subscribe_status)
		{
			case SUBSCRIBE:
				/* subscribe */
				topicString.cstring = topic;
				len = MQTTSerialize_subscribe(buf, buflen, 0, packetid, 1, &topicString, &req_qos);
				rc = transport_sendPacketBuffer(mysock, buf, len);
				subscribe_status = SUBACK;
			break;
			
			case SUBACK:
				if (MQTTPacket_read(buf, buflen, transport_getdata) == SUBACK) 	/* wait for suback */
				{
					unsigned short submsgid;
					int subcount;
					int granted_qos;

					rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, buf, buflen);
					USART_OUT(USART1, "granted qos =%d\n", granted_qos);	
					ret = 1;
				}
			break;
				
			default:
			break;	
			
		}
	}
	
	
	return ret;
}




int mqtt_keep_alive(void)
{
	int ret = 0;
	int rc = 0;
	int mysock = 0;
	int len = 0;
	unsigned char buf[20];
	int buflen = sizeof(buf);
	
	
	len = MQTTSerialize_pingreq(buf, buflen);
	rc = transport_sendPacketBuffer(mysock, buf, len);
	
	timer_is_timeout_1ms(timer_mqtt_keep_alive, 0);
	while(!ret)
	{
		usart2_recv_data();	
		if(timer_is_timeout_1ms(timer_mqtt_keep_alive, 2000) == 0)
		{
			len = MQTTSerialize_pingreq(buf, buflen);
			rc = transport_sendPacketBuffer(mysock, buf, len);
		}

		rc = MQTTPacket_read(buf, buflen, transport_getdata);
		if(rc == PINGRESP)
		{
			USART_OUT(USART1, "mqtt_keep_alive ok\r\n", buf);
			ret = 1;
		}	
	}
	
	return ret;
}







void mqtt_pub_qos0(void)
{
	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	int rc = 0;
	char buf[200];
	int buflen = sizeof(buf);
	int mysock = 0;
	MQTTString topicString = MQTTString_initializer;
	char* payload = "mypayload";
	int payloadlen = strlen(payload);
	int len = 0;
	char *host = "m2m.eclipse.org";
	int port = 1883;

//	if (argc > 1)
//		host = argv[1];

//	if (argc > 2)
//		port = atoi(argv[2]);

//	mysock = transport_open(host,port);
//	if(mysock < 0)
//		return mysock;


	USART_OUT(USART1, "Sending to hostname %s port %d\n", host, port);
	
	data.clientID.cstring = "me";
	data.keepAliveInterval = 20;
	data.cleansession = 1;
	data.username.cstring = "testuser";
	data.password.cstring = "testpassword";
	data.MQTTVersion = 4;

	len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);

	topicString.cstring = "mytopic";
	len += MQTTSerialize_publish((unsigned char *)(buf + len), buflen - len, 0, 0, 0, 0, topicString, (unsigned char *)payload, payloadlen);

	len += MQTTSerialize_disconnect((unsigned char *)(buf + len), buflen - len);

	rc = transport_sendPacketBuffer(mysock, (unsigned char*)buf, len);
	if (rc == len)
		USART_OUT(USART1, "Successfully published\n");
	else
		USART_OUT(USART1, "Publish failed\n");

}











