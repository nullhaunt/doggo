#pragma once

// Copy constructors and copy assignment operator are prohibited.
#define DOGGO_DISALLOW_COPY( type ) \
                                    \
private:                            \
  type( const type & );             \
  type & operator=( const type & )

//  Move constructors and move assignment operators are prohibited.
#define DOGGO_DISALLOW_MOVE( type )    \
                                       \
private:                               \
  type( type && ) noexcept;            \
  type & operator=( type && ) noexcept
