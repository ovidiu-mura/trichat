/* CS510 - ALSP
 * Team: Alex Davidoff, Kamakshi Nagar, Ovidiu Mura
 * Date: 08/13/2019
 *
 * It contains the header files needed for the implementation.
 **/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include<stdbool.h>
#include <libgen.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <aio.h>
#include <fcntl.h>
#include <stdbool.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "packets.h"
