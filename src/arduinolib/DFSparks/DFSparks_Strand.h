#ifndef DFSPARKS_STRAND_H
#define DFSPARKS_STRAND_H

namespace dfsparks {
	class Strand {
	public: 
	   Strand() : len(0), colors(nullptr) {}
	   ~Strand() {}
	   
	   int begin(int length);
	   void end();

	   int update(void *data, size_t size);

	   int length() const;
	   uint8_t red(int pixel) const;
	   uint8_t green(int pixel) const;
	   uint8_t blue(int pixel) const;
	   uint32_t color(int pixel) const;

	private:
		int len;
		uint32_t sequence;
	   	uint32_t *colors;
	};
} // namespace dfsparks

#endif /* DFSPARKS_STRAND_H */