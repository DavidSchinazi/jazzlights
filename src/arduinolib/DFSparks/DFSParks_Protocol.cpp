#include "DFSparks_Protocol.h"
#ifndef ARDUINO
# include <arpa/inet.h>
#else 
	inline uint32_t ntohl(uint32_t n) {
		return ((n & 0xFF) << 24)  | 
			   ((n & 0xFF00) << 8) |
			   ((n & 0xFF0000) >> 8) |
			   ((n & 0xFF000000) >> 24);
	}

	inline uint32_t ntohl(uint32_t n) {
		((n & 0xFF) << 24) | 
        ((n & 0xFF00) << 8) | 
        ((n & 0xFF0000) >> 8) | 
        ((n & 0xFF000000) >> 24));
	}
#endif

namespace dfsparks {
	namespace frame {

	  	struct Header {
	    	uint16_t protocol_version;
	     	uint32_t type;
	     	uint32_t size;
	  	} __attribute__ ((__packed__));   

      	Type decode_type(void *buf, size_t bufsize) {
      		if (bufsize < sizeof(Header)) {
      			return Type::UNKNOWN;
      		}
      		Header &header = *static_cast<Header*>(buf);
      		return static_cast<Type>(ntohl(header.type));
      	}

		void encode_header(Header& header, size_t wire_size, Type type) {
			header.type = htonl(static_cast<uint32_t>(type));
			header.size = htonl(wire_size);
		};

	   namespace solid {
		   	struct Frame {
		      	Header header;
		    	uint32_t color;
		   	} __attribute__ ((__packed__));
		    
		    static_assert(sizeof(Frame) <= max_size, "solid frame size is larger then max_size");
	        
	        size_t encode(void *buf, size_t bufsize, uint32_t color) {
	        	if (bufsize < sizeof(Frame)) {
	        		return 0;
	        	}
			    Frame &frame = *static_cast<Frame*>(buf);
			    encode_header(frame.header, sizeof(Frame), Type::SOLID);
			    return sizeof(Frame);
	        }

    	    size_t decode(void *buf, size_t bufsize, uint32_t *color) {
    	    	if (bufsize < sizeof(Frame) || 
    	    		decode_type(buf, bufsize) != Type::SOLID) {
    	    		return 0;
    	    	}
			    Frame &frame = *static_cast<Frame*>(buf);
				*color = ntohl(frame.color);
			    return sizeof(Frame);
    	    }
	   } // namespace solid


	} // namespace frame
} // namespace dfsparks

