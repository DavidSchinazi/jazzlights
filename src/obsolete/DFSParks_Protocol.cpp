#include <string.h>
#include <stdint.h>
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

	inline uint32_t htonl(uint32_t n) {
		return ((n & 0xFF) << 24) | 
        	   ((n & 0xFF00) << 8) | 
               ((n & 0xFF0000) >> 8) | 
               ((n & 0xFF000000) >> 24);
	}
#endif

static const uint32_t MX = UINT32_MAX/2;

inline uint32_t enc_unit(double v) {
	return htonl(MX*v);
}

inline double dec_unit(uint32_t v) {
	return 1.0*ntohl(v)/MX;
}

namespace dfsparks {
	namespace frame {

	  	struct Header {
	  		char	 proto_name[4];
	     	uint8_t  proto_version_major;
	     	uint8_t  proto_version_minor;
	     	uint32_t type;
	     	uint32_t size;
	  	} __attribute__ ((__packed__));   

      	Type decode_type(void *buf, size_t bufsize) {
      		if (bufsize < sizeof(Header)) {
      			return Type::UNKNOWN;
      		}
      		Header &header = *static_cast<Header*>(buf);
      		bool same_proto = !strncmp(header.proto_name, proto_name, 
      			sizeof(header.proto_name));
      		bool same_version = header.proto_version_major == proto_version_major;
      		if (!same_proto || !same_version) {
      			return Type::UNKNOWN;
      		}
      		return static_cast<Type>(ntohl(header.type));
      	}

		void encode_header(Header& header, size_t wire_size, Type type) {
			memset(&header, 0, wire_size);
			strncpy(header.proto_name, proto_name, sizeof(header.proto_name));
			header.proto_version_major = proto_version_major;
			header.proto_version_minor = proto_version_minor; 
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
			    frame.color = htonl(color);
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

	   namespace random {
		   	struct Frame {
		      	Header header;
		    	uint32_t sequence;
		   	} __attribute__ ((__packed__));
		    
		    static_assert(sizeof(Frame) <= max_size, "random frame size is larger then max_size");
	        
	        size_t encode(void *buf, size_t bufsize, uint32_t sequence) {
	        	if (bufsize < sizeof(Frame)) {
	        		return 0;
	        	}
			    Frame &frame = *static_cast<Frame*>(buf);
			    encode_header(frame.header, sizeof(Frame), Type::RANDOM);
			    frame.sequence = htonl(sequence);
			    return sizeof(Frame);
	        }

    	    size_t decode(void *buf, size_t bufsize, uint32_t *sequence) {
    	    	if (bufsize < sizeof(Frame) || 
    	    		decode_type(buf, bufsize) != Type::RANDOM) {
    	    		return 0;
    	    	}
			    Frame &frame = *static_cast<Frame*>(buf);
				*sequence = ntohl(frame.sequence);
			    return sizeof(Frame);
    	    }
	   } // namespace random

	   namespace rainbow {
		   	struct Frame {
		      	Header header;
		    	uint32_t from_hue;
		    	uint32_t to_hue;
		    	uint32_t saturation;
		    	uint32_t lightness;
		   	} __attribute__ ((__packed__));
		    
		    static_assert(sizeof(Frame) <= max_size, "rainbow frame size is larger then max_size");
	        
	        size_t encode(void *buf, size_t bufsize, double from_hue, double to_hue, double saturation, double lightness) {
	        	if (bufsize < sizeof(Frame)) {
	        		return 0;
	        	}
			    Frame &frame = *static_cast<Frame*>(buf);
			    encode_header(frame.header, sizeof(Frame), Type::RAINBOW);
                frame.from_hue = enc_unit(from_hue);
			    frame.to_hue = enc_unit(to_hue);
			    frame.saturation = enc_unit(saturation);
			    frame.lightness = enc_unit(lightness);
			    return sizeof(Frame);
	        }

    	    size_t decode(void *buf, size_t bufsize, double *from_hue, double *to_hue, double *saturation, double *lightness) {
    	    	if (bufsize < sizeof(Frame) || 
    	    		decode_type(buf, bufsize) != Type::RAINBOW) {
    	    		return 0;
    	    	}
			    Frame &frame = *static_cast<Frame*>(buf);
				*from_hue = dec_unit(frame.from_hue);
				*to_hue = dec_unit(frame.to_hue);
				*saturation = dec_unit(frame.saturation);
				*lightness = dec_unit(frame.lightness);
			    return sizeof(Frame);
    	    }
	   } // namespace random

	} // namespace frame
} // namespace dfsparks

