#ifndef CONSTANTS_H
#define CONSTANTS_H

#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

#define HERR(source) (fprintf(stderr,"%s(%d) at %s:%d\n",source,h_errno,__FILE__,__LINE__),\
	exit(EXIT_FAILURE))

#define BUFF_SIZE 1024
#define HEADER_SIZE 12

#endif
