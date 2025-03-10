#ifndef WATER_TORTURE_HPP_
#define WATER_TORTURE_HPP_
#include "rgb_operators.h"
#include <Adafruit_NeoPixel.h>
#include <string.h>

namespace
{
  using ws2811::rgb;

  uint8_t mult( uint8_t value, uint16_t multiplier)
  {
    return (static_cast<uint16_t>( value) * multiplier) >> 8;
  }

  /// This class maintains the state and calculates the animations to render a falling water droplet
  /// Objects of this class can have three states:
  ///    - inactive: this object does nothing
  ///    - swelling: the droplet is at the top of the led strip and swells in intensity
  ///    - falling: the droplet falls downwards and accelerates
  ///    - bouncing: the droplet has bounced of the ground. A smaller, less intensive droplet bounces up
  ///      while a part of the drop remains on the ground.
  /// After going through the swelling, falling and bouncing phases, the droplet automatically returns to the
  /// inactive state.
  class droplet
  {
  public:
    droplet( const rgb &color, uint16_t gravity)
    :color( color), position(0), speed(0),
     gravity( gravity), state(swelling)
    {}

    droplet()
    :color(0,0,0), position(0), speed(0),
     gravity(0), state( inactive)
    {

    }
    /// calculate the next step in the animation for this droplet
    void step( uint8_t maxpos)
    {
      if (state == falling || state == bouncing)
      {
        position += speed;
        speed += gravity;

        // if we hit the bottom...
        const uint16_t maxpos16 = maxpos << 8;
        if (position > maxpos16)
        {
          if (state == bouncing)
          {
            // this is the second collision,
            // deactivate.
            state = inactive;
          }
          else
          {
            // reverse direction and dampen the speed
            position = maxpos16 - (position - maxpos16);
            speed = -speed/4;
            color = scale( color, 10);
            state = bouncing;
          }
        }
      }
      else if (state == swelling)
      {
        ++position;
        if ( color.blue <= 10 || color.blue - position <= 10)
        {
          state = falling;
          position = 0;
        }

      }
    }

    /// perform one step and draw.
    void step( rgb *leds, uint8_t ledcount, bool reverse)
    {
      step( ledcount - 1);
      draw( leds, ledcount - 1, reverse);
    }

    /// Draw the droplet on the led string
    /// This will "smear" the light of this droplet between two leds. The closer
    /// the droplets position is to that of a particular led, the brighter that
    /// led will be
    void draw( rgb *leds, uint8_t max_pos, bool reverse)
    {
      if (state == falling || state == bouncing)
      {
        uint8_t position8 = position >> 8;
        uint8_t remainder = position; // get the lower bits

        uint8_t last = max_pos;
        uint8_t pos = position8;
        uint8_t pos1 = position8+1;

        if (reverse)
        {
          last = 0;
          pos = max_pos-position8;
          pos1 = pos-1;
        }
        else
        {
          last = max_pos;
          pos = position8;
          pos1 = pos+1;
        }

        add_clipped_to( leds[pos], scale( color, 256 - remainder ));
        if (remainder)
        {
          add_clipped_to( leds[pos1], scale( color, remainder));
        }

        if (state == bouncing)
        {
          add_clipped_to( leds[last], color);
        }
      }
      else if (state == swelling)
      {
        uint8_t first;
        if (reverse)
        {
          first = max_pos;
        }
        else
        {
          first = 0;
        }
        add_clipped_to( leds[first], scale( color, position));
      }
    }

    bool is_active() const
    {
      return state != inactive;
    }

private:
    /// Add  two numbers and clip the result at 255.
    static uint8_t add_clipped( uint16_t left, uint16_t right)
    {
      uint16_t result = left + right;
      if (result > 255) result = 255;
      return result;
    }

    /// Add the right rgb value to the left one, clipping if necessary
    static void add_clipped_to( rgb &left, const rgb &right)
    {
          left.red   = add_clipped(left.red, right.red);
          left.green = add_clipped( left.green, right.green);
          left.blue  = add_clipped( left.blue, right.blue);
    }

    /// multiply an 8-bit value with an 8.8 bit fixed point number.
    /// multiplier should not be higher than 1.00 (or 256).
    static uint8_t mult( uint8_t value, uint16_t multiplier)
    {
      return (static_cast<uint16_t>( value) * multiplier) >> 8;
    }

    /// scale an rgb value up or down. amplitude > 256 means scaling up, while
    /// amplitude < 256 means scaling down.
    static rgb scale(rgb value, uint16_t amplitude)
    {
      return rgb(
          mult( value.red, amplitude),
          mult( value.green, amplitude),
          mult( value.blue, amplitude)
      );
    }

    // how much of a color is left when colliding with the floor, value
    // between 0 and 256 where 256 means no loss.
    static const uint16_t collision_scaling = 40;
    rgb    color;
    uint16_t position;
    int16_t  speed;
    uint16_t gravity;
    enum stateval {
      inactive,
      swelling,
      falling,
      bouncing
    };

    stateval state;
  };

  uint8_t debugcount = 0;
  volatile uint16_t random_scale()
  {
    return (rand() % 256);
  }

  // Array of colors to cycle through
  static const rgb droplet_colors[] = {
      rgb(255, 255, 0),     // Yellow
      rgb(255, 0, 255),     // Purple (richer purple)
      rgb(128, 128, 128),   // Gray
      rgb(255, 255, 255),   // White
      rgb(0, 0, 255)      // Blue
  };
  static const uint8_t num_droplet_colors = sizeof(droplet_colors) / sizeof(droplet_colors[0]);
  static uint8_t current_color_index = 0; // Keep track of the current color

  void create_random_droplet( droplet &d)
  {
    d = droplet(
        droplet_colors[current_color_index], // Use color from the array
        5);
    current_color_index = (current_color_index + 1) % num_droplet_colors; // Cycle to the next color
  }

}

class WaterTorture
{
  public:

  WaterTorture( Adafruit_NeoPixel *strip)
  {
    this->strip = strip;
      current_droplet = 0; // index of the next droplet to be created
      droplet_pause = 1; // how long to wait for the next one
  }

void animate (bool reverse)
  {
        if (droplet_pause)
        {
          --droplet_pause;
        }
        else
        {
          if (!droplets[current_droplet].is_active())
          {
            create_random_droplet( droplets[current_droplet]);
            ++current_droplet;
            if (current_droplet >= droplet_count) current_droplet = 0;
            droplet_pause = 100 + rand() % 80;
          }
        }

        rgb *leds = (rgb *)strip->getPixels();
        memset( leds, 0, strip->numPixels()*sizeof(rgb));
        for (uint8_t idx = 0; idx < droplet_count; ++idx)
        {
          droplets[idx].step( leds, strip->numPixels(), reverse);
        }

//  for (uint8_t i = 0; i < strip->numPixels(); i++)
//  {
//    strip->setPixelColor(i, leds->red, leds->green, leds->blue); // overwrite with brightness
//    leds++;
//  }
  }

private:
      Adafruit_NeoPixel *strip;
      static const uint8_t droplet_count = 4;
      droplet droplets[droplet_count]; // droplets that can animate simultaneously.
      uint8_t current_droplet; // index of the next droplet to be created
      uint8_t droplet_pause; // how long to wait for the next one
};

#endif /* WATER_TORTURE_HPP_ */
