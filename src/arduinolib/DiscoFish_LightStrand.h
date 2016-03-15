/*
 * DFPixel.h - Library for creating wearable gadgets synchronized with DiscoFish 
 * art car.
 */
#ifndef DISCOFISH_WEARABLES_H
#define DISCOFISH_WEARABLES_H

class DiscoFish_LightStrand {
public: 
   DiscoFish_LightStrand(int pixelCount);
   ~DiscoFish_LightStrand();

   int loadFrame(void *data, size_t size);

   uint8_t getRed(int pixel) const;
   uint8_t getGreen(int pixel) const;
   uint8_t getBlue(int pixel) const;

private:
   uint32_t *colors;
};

#endif /* DISCOFISH_WEARABLES_H */