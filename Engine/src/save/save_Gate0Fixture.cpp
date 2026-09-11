#include "doggo/save/save_Gate0Fixture.hpp"

#include <array>
#include <limits>
#include <span>

namespace
{
  constexpr std::size_t GenerationOffset = 16;
  constexpr std::size_t ChecksumOffset   = 56;

  [[nodiscard]] constexpr std::uint64_t fnv1a64( const std::span<const std::uint8_t> bytes ) noexcept
  {
    std::uint64_t hash = 0xCBF29CE484222325;
    for ( const std::uint8_t byte : bytes )
    {
      hash ^= byte;
      hash *= 0x100000001B3;
    }

    return hash;
  }

  constexpr void writeLittleEndian32( std::array<std::uint8_t, doggo::save::Gate0FixtureSize> & destination,
                                      const std::size_t                                         offset,
                                      const std::uint32_t                                       value ) noexcept
  {
    for ( std::size_t index = 0; index < sizeof( value ); ++index )
    {
      destination[ offset + index ] = static_cast<std::uint8_t>( value >> ( index * 8 ) );
    }
  }

  constexpr void writeLittleEndian64( std::array<std::uint8_t, doggo::save::Gate0FixtureSize> & destination,
                                      const std::size_t                                         offset,
                                      const std::uint64_t                                       value ) noexcept
  {
    for ( std::size_t index = 0; index < sizeof( value ); ++index )
    {
      destination[ offset + index ] = static_cast<std::uint8_t>( value >> ( index * 8 ) );
    }
  }

  [[nodiscard]] constexpr std::uint64_t
  readLittleEndian64( const std::array<std::uint8_t, doggo::save::Gate0FixtureSize> & source,
                      const std::size_t                                               offset ) noexcept
  {
    std::uint64_t value = 0;
    for ( std::size_t index = 0; index < sizeof( value ); ++index )
    {
      value |= static_cast<std::uint64_t>( source[ offset + index ] ) << ( index * 8 );
    }

    return value;
  }

  [[nodiscard]] constexpr std::array<std::uint8_t, doggo::save::Gate0FixtureSize>
  makeFixture( const std::uint64_t generation ) noexcept
  {
    constexpr std::array<std::uint8_t, 8> magic = { 0x44, 0x4F, 0x47, 0x47, 0x4F, 0x53, 0x41, 0x56 };

    std::array<std::uint8_t, doggo::save::Gate0FixtureSize> fixture = {};
    for ( std::size_t index = 0; index < magic.size(); ++index )
    {
      fixture[ index ] = magic[ index ];
    }

    writeLittleEndian32( fixture, 8, 1 );
    writeLittleEndian32( fixture, 12, doggo::save::Gate0FixtureSize );
    writeLittleEndian64( fixture, GenerationOffset, generation );

    for ( std::size_t index = 0; index < 32; ++index )
    {
      fixture[ 24 + index ] = static_cast<std::uint8_t>( 0xA0 + index );
    }

    const std::uint64_t checksum = fnv1a64( std::span<const std::uint8_t>{ fixture.data(), ChecksumOffset } );
    writeLittleEndian64( fixture, ChecksumOffset, checksum );
    return fixture;
  }

  constexpr auto FirstFixture = makeFixture( 1 );
  static_assert( readLittleEndian64( FirstFixture, ChecksumOffset ) == 0x65FCF37B61C6B04B );
  static_assert( fnv1a64( std::span<const std::uint8_t>{ FirstFixture } ) == 0xCC89D9B280F89DE4 );
}  // namespace

namespace doggo::save
{
  Gate0FixtureReport runGate0Fixture( SaveStorage & storage ) noexcept
  {
    Gate0FixtureReport report;
    report.storage.native_result = storage.initialize();
    if ( report.storage.native_result != 0 )
    {
      return report;
    }

    std::array<std::uint8_t, Gate0FixtureSize> previousFixture = {};
    report.storage = storage.readCommittedFile( Gate0FixtureName, previousFixture );
    if ( report.storage.status == SaveStorageStatus::Success )
    {
      report.had_previous_fixture = true;
      report.previous_generation  = readLittleEndian64( previousFixture, GenerationOffset );
      const auto expectedPrevious = makeFixture( report.previous_generation );
      if ( report.previous_generation == 0 ||
           report.storage.file_size != previousFixture.size() ||
           report.storage.bytes_transferred != previousFixture.size() ||
           previousFixture != expectedPrevious )
      {
        report.status = Gate0FixtureStatus::PreviousFixtureInvalid;
        report.content_hash =
            fnv1a64( std::span<const std::uint8_t>{ previousFixture.data(), report.storage.bytes_transferred } );
        return report;
      }
    }
    else if ( report.storage.status != SaveStorageStatus::NotFound )
    {
      report.status = Gate0FixtureStatus::PreviousReadFailed;
      return report;
    }

    if ( report.previous_generation == std::numeric_limits<std::uint64_t>::max() )
    {
      report.status = Gate0FixtureStatus::GenerationExhausted;
      return report;
    }

    report.committed_generation = report.previous_generation + 1;
    const auto expectedFixture  = makeFixture( report.committed_generation );
    report.storage              = storage.writeCommittedFile( Gate0FixtureName, expectedFixture );
    if ( report.storage.status != SaveStorageStatus::Success )
    {
      report.status = Gate0FixtureStatus::WriteFailed;
      return report;
    }

    std::array<std::uint8_t, Gate0FixtureSize> readBackFixture = {};
    report.storage = storage.readCommittedFile( Gate0FixtureName, readBackFixture );
    if ( report.storage.status != SaveStorageStatus::Success )
    {
      report.status = Gate0FixtureStatus::ReadBackFailed;
      return report;
    }

    report.content_hash =
        fnv1a64( std::span<const std::uint8_t>{ readBackFixture.data(), report.storage.bytes_transferred } );
    const bool isExactSize = report.storage.file_size == expectedFixture.size() &&
                             report.storage.bytes_transferred == expectedFixture.size();
    if ( !isExactSize || readBackFixture != expectedFixture )
    {
      report.status = Gate0FixtureStatus::ReadBackInvalid;
      return report;
    }

    report.status = Gate0FixtureStatus::Success;
    return report;
  }
}  // namespace doggo::save
