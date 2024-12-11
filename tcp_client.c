#include "xparameters.h"
#include "netif/xadapter.h"
#include "lwip/tcp.h"
#include "xil_printf.h"
#include "platform.h"

#define SERVER_IP_ADDR "192.168.1.100"  // Replace with your PC's IP
#define SERVER_PORT 7000

static struct tcp_pcb *client_pcb = NULL;
static int connection_state = 0;

err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    xil_printf("Connected to server\r\n");
    connection_state = 1;
    
    // Prepare message
    char send_buffer[] = "Hello World";
    
    // Send data
    err = tcp_write(tpcb, send_buffer, strlen(send_buffer), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        xil_printf("Error writing to socket\r\n");
        return err;
    }
    
    tcp_output(tpcb);
    return ERR_OK;
}

err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    
    // Process received data
    char *rx_buffer = (char *)p->payload;
    xil_printf("Received from server: %s\r\n", rx_buffer);
    
    pbuf_free(p);
    return ERR_OK;
}

void tcp_client_err(void *arg, err_t err) {
    xil_printf("TCP Client Error: %d\r\n", err);
}

int tcp_client_init() {
    // Create new TCP PCB
    client_pcb = tcp_new();
    if (!client_pcb) {
        xil_printf("Error creating PCB\r\n");
        return -1;
    }
    
    // Bind callbacks
    tcp_arg(client_pcb, NULL);
    tcp_err(client_pcb, tcp_client_err);
    tcp_recv(client_pcb, tcp_client_recv);
    tcp_sent(client_pcb, NULL);
    
    // Server address structure
    ip_addr_t server_addr;
    if (inet_aton(SERVER_IP_ADDR, &server_addr) == 0) {
        xil_printf("Invalid IP address\r\n");
        return -1;
    }
    
    // Attempt connection
    err_t err = tcp_connect(client_pcb, &server_addr, SERVER_PORT, tcp_client_connected);
    if (err != ERR_OK) {
        xil_printf("Connection error\r\n");
        return -1;
    }
    
    return 0;
}

int main() {
    init_platform();
    
    // Initialize network interface
    xil_printf("Initializing Network Interface...\r\n");
    network_init();
    
    // Initialize TCP Client
    while (tcp_client_init() != 0) {
        xil_printf("Retrying client initialization...\r\n");
        msleep(1000);
    }
    
    // Main event loop
    while (1) {
        // Process network events
        xemacif_input(netif);
        
        // Add any additional logic here
    }
    
    cleanup_platform();
    return 0;
}

/*network init()*/
#include "xparameters.h"
#include "netif/xadapter.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/inet.h"
#include "platform_config.h"

// Network interface structure
static struct netif server_netif;

void network_init() {
    // Initialize LwIP stack
    lwip_init();

    // Add network interface
    ip_addr_t ipaddr, netmask, gw;

    // Static IP Configuration
    IP4_ADDR(&ipaddr,  192, 168, 1, 101);   // ZCU102 IP
    IP4_ADDR(&netmask, 255, 255, 255, 0);   // Subnet mask
    IP4_ADDR(&gw,      192, 168, 1, 1);     // Gateway

    // Initialize the network interface
    if (!xemac_initialize(&server_netif, 
                          XPAR_ETHERNET_MAC_BASEADDR, 
                          &ipaddr, 
                          &netmask, 
                          &gw)) {
        xil_printf("Error in network interface initialization\r\n");
        return;
    }

    // Set the default network interface
    netif_set_default(&server_netif);

    // Bring up the network interface
    netif_set_up(&server_netif);
}

int main() {
    init_platform();
    
    // Initialize network interface
    xil_printf("Initializing Network Interface...\r\n");
    network_init();
    
    // Initialize TCP Client
    while (tcp_client_init() != 0) {
        xil_printf("Retrying client initialization...\r\n");
        msleep(1000);
    }
    
    // Tracking variables
    int connection_attempts = 0;
    int max_connection_attempts = 5;
    
    // Main event loop
    while (1) {
        // Process incoming network packets
        xemacif_input(netif);
        
        // Connection management
        if (!connection_state) {
            connection_attempts++;
            if (connection_attempts >= max_connection_attempts) {
                xil_printf("Max connection attempts reached. Reinitializing...\r\n");
                tcp_client_init();
                connection_attempts = 0;
            }
        }
        
        // Optional: Periodic heartbeat or status check
        static int heartbeat_counter = 0;
        heartbeat_counter++;
        if (heartbeat_counter >= 10000) {
            xil_printf("System alive. Connection state: %d\r\n", connection_state);
            heartbeat_counter = 0;
        }
        
        // Optional: Add application-specific tasks
        // For example, sensor reading, state machine, etc.
        
        // Prevent tight loop from consuming 100% CPU
        usleep(100);  // Small delay
    }
    
    cleanup_platform();
    return 0;
}
