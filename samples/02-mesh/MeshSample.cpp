#include "MeshSample.hpp"

#include <cmath>
#include <numbers>

#include "diagnostics/Assert.hpp"

namespace doggo::sample::mesh
{
    namespace
    {
        struct Mat4
        {
            std::array<float, 16> m = {};
        };

        [[nodiscard]] Mat4 makeIdentity() noexcept
        {
            Mat4 result = {};

            result.m[ 0 ]  = 1.0f;
            result.m[ 5 ]  = 1.0f;
            result.m[ 10 ] = 1.0f;
            result.m[ 15 ] = 1.0f;

            return result;
        }

        [[nodiscard]] Mat4 multiply( const Mat4 & a, const Mat4 & b ) noexcept
        {
            Mat4 result = {};

            for ( std::size_t column = 0; column < 4; ++column )
            {
                for ( std::size_t row = 0; row < 4; ++row )
                {
                    result.m[ column * 4 + row ] =
                        a.m[ 0 * 4 + row ] * b.m[ column * 4 + 0 ] + a.m[ 1 * 4 + row ] * b.m[ column * 4 + 1 ] +
                        a.m[ 2 * 4 + row ] * b.m[ column * 4 + 2 ] + a.m[ 3 * 4 + row ] * b.m[ column * 4 + 3 ];
                }
            }

            return result;
        }

        [[nodiscard]] Mat4 makeTranslation( float x, float y, float z ) noexcept
        {
            Mat4 result = makeIdentity();

            result.m[ 12 ] = x;
            result.m[ 13 ] = y;
            result.m[ 14 ] = z;

            return result;
        }

        [[nodiscard]] Mat4 makeRotationX( float radians ) noexcept
        {
            Mat4 result = makeIdentity();

            const float cosine = std::cos( radians );
            const float sine   = std::sin( radians );

            result.m[ 5 ]  = cosine;
            result.m[ 6 ]  = sine;
            result.m[ 9 ]  = -sine;
            result.m[ 10 ] = cosine;

            return result;
        }

        [[nodiscard]] Mat4 makeRotationY( float radians ) noexcept
        {
            Mat4 result = makeIdentity();

            const float cosine = std::cos( radians );
            const float sine   = std::sin( radians );

            result.m[ 0 ]  = cosine;
            result.m[ 2 ]  = -sine;
            result.m[ 8 ]  = sine;
            result.m[ 10 ] = cosine;

            return result;
        }

        [[nodiscard]] Mat4 makePerspectiveRhZo( float verticalFovRadians,
                                                float aspectRatio,
                                                float nearPlane,
                                                float farPlane ) noexcept
        {
            DOGGO_ASSERT( verticalFovRadians > 0.0f );
            DOGGO_ASSERT( aspectRatio > 0.0f );
            DOGGO_ASSERT( nearPlane > 0.0f );
            DOGGO_ASSERT( farPlane > nearPlane );

            const float focalLength = 1.0f / std::tan( verticalFovRadians * 0.5f );

            Mat4 result = {};

            result.m[ 0 ]  = focalLength / aspectRatio;
            result.m[ 5 ]  = focalLength;
            result.m[ 10 ] = farPlane / ( nearPlane - farPlane );
            result.m[ 11 ] = -1.0f;
            result.m[ 14 ] = ( farPlane * nearPlane ) / ( nearPlane - farPlane );

            return result;
        }

        constexpr std::array<render::MeshVertex, 8> CubeVertices{ {
            { .mPosition = { -1.0F, -1.0F, -1.0F }, .mColor = { 1.0F, 0.2F, 0.2F } },
            { .mPosition = { 1.0F, -1.0F, -1.0F }, .mColor = { 0.2F, 1.0F, 0.2F } },
            { .mPosition = { 1.0F, 1.0F, -1.0F }, .mColor = { 0.2F, 0.4F, 1.0F } },
            { .mPosition = { -1.0F, 1.0F, -1.0F }, .mColor = { 1.0F, 1.0F, 0.2F } },

            { .mPosition = { -1.0F, -1.0F, 1.0F }, .mColor = { 1.0F, 0.2F, 1.0F } },
            { .mPosition = { 1.0F, -1.0F, 1.0F }, .mColor = { 0.2F, 1.0F, 1.0F } },
            { .mPosition = { 1.0F, 1.0F, 1.0F }, .mColor = { 1.0F, 1.0F, 1.0F } },
            { .mPosition = { -1.0F, 1.0F, 1.0F }, .mColor = { 1.0F, 0.5F, 0.2F } },
        } };

        constexpr std::array<std::uint16_t, 36> CubeIndices{ {
            // clang-format off
            // Front
            4, 5, 6,
            6, 7, 4,

            // Back
            1, 0, 3,
            3, 2, 1,

            // Left
            0, 4, 7,
            7, 3, 0,

            // Right
            5, 1, 2,
            2, 6, 5,

            // Top
            3, 7, 6,
            6, 2, 3,

            // Bottom
            0, 1, 5,
            5, 4, 0,
            // clang-format on
        } };
    } // namespace

    MeshSample::MeshSample() noexcept
        : mMeshData{ .mVertices = CubeVertices, .mIndices = CubeIndices }
    {
    }

    render::DrawData MeshSample::buildDrawData( const render::FrameInfo & frameInfo ) const noexcept
    {
        constexpr float DegreesToRadians = std::numbers::pi_v<float> / 180.0f;

        static const Mat4 viewProjection =
            multiply( makePerspectiveRhZo( 60.0f * DegreesToRadians, 1280.0f / 720.0f, 0.1f, 100.0f ),
                      makeTranslation( 0.0f, 0.0f, -4.0f ) );

        const float seconds = std::chrono::duration<float>{ frameInfo.mElapsedTime }.count();
        const Mat4  model   = multiply( makeRotationY( seconds * 0.8f ), makeRotationX( -20.0f * DegreesToRadians ) );
        const Mat4  mvp     = multiply( viewProjection, model );

        return render::DrawData{ .mModelViewProjection = mvp.m };
    }

    const render::MeshData & MeshSample::getMeshData() const noexcept
    {
        return mMeshData;
    }
} // namespace doggo::sample::mesh
