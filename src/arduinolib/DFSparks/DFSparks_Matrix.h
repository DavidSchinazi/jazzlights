#ifndef DFSPARKS_MATRIX_H
#define DFSPARKS_MATRIX_H
#include <DFSparks_Network.h>
#include <DFSparks_Effects.h>
#include <DFSparks_Pixels.h>
#include <DFSparks_Player.h>

namespace dfsparks {

template <int width, int height> class Matrix {
public:
  Matrix() : renderer(*this), pixels(renderer), player(all_effects) {}

  ~Matrix() {}

  void begin() { client = server = nullptr; }

  void begin(Client &cli) {
    cli.begin();
    client = &cli;
    server = nullptr;
  }

  void begin(Server &srv) {
    srv.begin();
    client = nullptr;
    server = &srv;
  }

  void render() {
    if (client) {
      client->update();
      player.mirror(client->get_effect_id(), client->get_start_time());
    }
    player.render(pixels);
    if (server) {
      server->update(player.get_playing_effect_id(), player.get_elapsed_time());
    }
  }

  void play_next() { player.play_next(); }

  void play_prev() { player.play_prev(); }

private:
  virtual void on_set_pixel_color(int x, int y, uint8_t r, uint8_t g,
                                  uint8_t b) = 0;

  struct Renderer {
    Renderer(Matrix<width, height> &m) : matrix(m) {}
    void operator()(int x, int y, uint32_t c) {
      matrix.on_set_pixel_color(x, y, chan_red(c), chan_green(c), chan_blue(c));
    }
    Matrix<width, height> &matrix;
  } renderer;

  typedef Pixels2D<width, height, Renderer> Pixels;
  Pixels pixels;
  Effects<Pixels> all_effects;
  Player<Pixels> player;
  Client *client = nullptr;
  Server *server = nullptr;
};

} // namespace dfsparks
#endif /* DFSPARKS_MATRIX_H_ */
