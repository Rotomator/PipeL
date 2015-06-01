//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include <Alembic/AbcGeom/OCurves.h>
#include <Alembic/AbcGeom/GeometryScope.h>

namespace Alembic {
namespace AbcGeom {
namespace ALEMBIC_VERSION_NS {

//-*****************************************************************************
void OCurvesSchema::set( const OCurvesSchema::Sample &iSamp )
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OCurvesSchema::set()" );

    Alembic::Util::uint8_t basisAndType[4];
    basisAndType[0] = iSamp.getType();
    basisAndType[1] = iSamp.getWrap();
    basisAndType[2] = iSamp.getBasis();

    // repeat so we don't have to change the data layout and bump up
    // the version number
    basisAndType[3] = basisAndType[2];

    // do we need to create velocities prop?
    if ( iSamp.getVelocities() && !m_velocitiesProperty )
    {
        m_velocitiesProperty = Abc::OV3fArrayProperty( this->getPtr(), ".velocities",
                                               m_positionsProperty.getTimeSampling() );

        std::vector<V3f> emptyVec;
        const V3fArraySample empty(emptyVec);
        const size_t numSamps = m_positionsProperty.getNumSamples();
        for ( size_t i = 0 ; i < numSamps ; ++i )
        {
            m_velocitiesProperty.set( empty );
        }
    }

    // do we need to create uvs?
    if ( iSamp.getUVs() && !m_uvsParam )
    {
        std::vector<V2f> emptyVals;
        std::vector<Util::uint32_t> emptyIndices;

        OV2fGeomParam::Sample empty;

        if ( iSamp.getUVs().getIndices() )
        {
            empty = OV2fGeomParam::Sample( Abc::V2fArraySample( emptyVals ),
                Abc::UInt32ArraySample( emptyIndices ),
                iSamp.getUVs().getScope() );

            // UVs are indexed
            m_uvsParam = OV2fGeomParam( this->getPtr(), "uv", true,
                                        empty.getScope(), 1,
                                        this->getTimeSampling() );
        }
        else
        {
            empty = OV2fGeomParam::Sample( Abc::V2fArraySample( emptyVals ),
                                           iSamp.getUVs().getScope() );

            // UVs are not indexed
            m_uvsParam = OV2fGeomParam( this->getPtr(), "uv", false,
                                   empty.getScope(), 1,
                                   this->getTimeSampling() );
        }

        size_t numSamples = m_positionsProperty.getNumSamples();

        // set all the missing samples
        for ( size_t i = 0; i < numSamples; ++i )
        {
            m_uvsParam.set( empty );
        }
    }

    // do we need to create normals?
    if ( iSamp.getNormals() && !m_normalsParam )
    {
        std::vector<V3f> emptyVals;
        std::vector<Util::uint32_t> emptyIndices;

        ON3fGeomParam::Sample empty;

        if ( iSamp.getNormals().getIndices() )
        {
            empty = ON3fGeomParam::Sample( Abc::V3fArraySample( emptyVals ),
                Abc::UInt32ArraySample( emptyIndices ),
                iSamp.getNormals().getScope() );

            // normals are indexed
            m_normalsParam = ON3fGeomParam( this->getPtr(), "N", true,
                empty.getScope(), 1, this->getTimeSampling() );
        }
        else
        {
            empty = ON3fGeomParam::Sample( Abc::V3fArraySample( emptyVals ),
                                           iSamp.getNormals().getScope() );

            // normals are not indexed
            m_normalsParam = ON3fGeomParam( this->getPtr(), "N", false,
                                        empty.getScope(), 1,
                                        this->getTimeSampling() );
        }

        size_t numSamples = m_positionsProperty.getNumSamples();

        // set all the missing samples
        for ( size_t i = 0; i < numSamples; ++i )
        {
            m_normalsParam.set( empty );
        }
    }

    // do we need to create widths?
    if ( iSamp.getWidths().getVals() && !m_widthsParam )
    {
        std::vector<float> emptyVals;
        std::vector<Util::uint32_t> emptyIndices;
        OFloatGeomParam::Sample empty;

        if ( iSamp.getWidths().getIndices() )
        {
            empty = OFloatGeomParam::Sample( Abc::FloatArraySample( emptyVals ),
                Abc::UInt32ArraySample( emptyIndices ),
                iSamp.getWidths().getScope() );

            // widths are indexed for some weird reason which is
            // technically ok, just wasteful
            m_widthsParam = OFloatGeomParam( this->getPtr(), "width", true,
                                             iSamp.getWidths().getScope(),
                                             1, this->getTimeSampling() );
        }
        else
        {
            empty = OFloatGeomParam::Sample( Abc::FloatArraySample( emptyVals ),
                                             iSamp.getWidths().getScope() );

            // widths are not indexed
            m_widthsParam = OFloatGeomParam( this->getPtr(), "width", false,
                                             iSamp.getWidths().getScope(), 1,
                                             this->getTimeSampling() );
        }

        size_t numSamples = m_positionsProperty.getNumSamples();

        // set all the missing samples
        for ( size_t i = 0; i < numSamples; ++i )
        {
            m_widthsParam.set( empty );
        }
    }

    // We could add sample integrity checking here.
    if ( m_positionsProperty.getNumSamples() == 0 )
    {
        // First sample must be valid on all points.
        ABCA_ASSERT( iSamp.getPositions(),
                     "Sample 0 must have valid data for all mesh components" );

        m_positionsProperty.set( iSamp.getPositions() );
        m_nVerticesProperty.set( iSamp.getCurvesNumVertices() );

        m_basisAndTypeProperty.set( basisAndType );

        if ( m_velocitiesProperty )
        { m_velocitiesProperty.set( iSamp.getVelocities() ); }

        if ( iSamp.getSelfBounds().isEmpty() )
        {
            // OTypedScalarProperty::set() is not referentially transparent,
            // so we need a a placeholder variable.
            Abc::Box3d bnds(
                ComputeBoundsFromPositions( iSamp.getPositions() )
                           );

            m_selfBoundsProperty.set( bnds );

        }
        else { m_selfBoundsProperty.set( iSamp.getSelfBounds() ); }

        // process uvs
        if ( iSamp.getUVs() )
        {
            m_uvsParam.set( iSamp.getUVs() );
        }

        // process normals
        if ( iSamp.getNormals() )
        {
            m_normalsParam.set( iSamp.getNormals() );
        }

        // process widths
        if ( iSamp.getWidths() )
        {
            m_widthsParam.set( iSamp.getWidths() );
        }

    }
    else
    {
        SetPropUsePrevIfNull( m_positionsProperty, iSamp.getPositions() );
        SetPropUsePrevIfNull( m_nVerticesProperty, iSamp.getCurvesNumVertices() );

        // if number of vertices were specified, then the basis and type
        // was specified
        if ( m_nVerticesProperty )
        {
            m_basisAndTypeProperty.set( basisAndType );
        }
        else
        {
            m_basisAndTypeProperty.setFromPrevious();
        }

        if ( m_velocitiesProperty )
        { SetPropUsePrevIfNull( m_velocitiesProperty, iSamp.getVelocities() ); }

        if ( m_uvsParam )
        { m_uvsParam.set( iSamp.getUVs() ); }

        if ( m_normalsParam )
        { m_normalsParam.set( iSamp.getNormals() ); }

        if ( m_widthsParam )
        { m_widthsParam.set( iSamp.getWidths() ); }

        // update bounds
        if ( iSamp.getSelfBounds().hasVolume() )
        {
            m_selfBoundsProperty.set( iSamp.getSelfBounds() );
        }
        else if ( iSamp.getPositions() )
        {
            Abc::Box3d bnds(
                ComputeBoundsFromPositions( iSamp.getPositions() )
                           );
            m_selfBoundsProperty.set( bnds );
        }
        else
        {
            m_selfBoundsProperty.setFromPrevious();
        }
    }

    ALEMBIC_ABC_SAFE_CALL_END();
}

//-*****************************************************************************
void OCurvesSchema::setFromPrevious()
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OCurvesSchema::setFromPrevious" );

    m_positionsProperty.setFromPrevious();
    m_nVerticesProperty.setFromPrevious();

    m_basisAndTypeProperty.setFromPrevious();

    m_selfBoundsProperty.setFromPrevious();

    if ( m_velocitiesProperty ) { m_velocitiesProperty.setFromPrevious(); }
    if ( m_uvsParam ) { m_uvsParam.setFromPrevious(); }
    if ( m_normalsParam ) { m_normalsParam.setFromPrevious(); }
    if ( m_widthsParam ) { m_widthsParam.setFromPrevious(); }

    ALEMBIC_ABC_SAFE_CALL_END();
}

//-*****************************************************************************
void OCurvesSchema::setTimeSampling( uint32_t iIndex )
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN(
        "OCurvesSchema::setTimeSampling( uint32_t )" );

    m_positionsProperty.setTimeSampling( iIndex );
    m_nVerticesProperty.setTimeSampling( iIndex );

    m_basisAndTypeProperty.setTimeSampling( iIndex );

    m_selfBoundsProperty.setTimeSampling( iIndex );

    if ( m_velocitiesProperty )
    {
        m_velocitiesProperty.setTimeSampling( iIndex );
    }

    if ( m_uvsParam ) { m_uvsParam.setTimeSampling( iIndex ); }

    if ( m_normalsParam ) { m_normalsParam.setTimeSampling( iIndex ); }
    if ( m_widthsParam ) { m_widthsParam.setTimeSampling( iIndex ); }

    m_positionsProperty.setTimeSampling( iIndex );

    ALEMBIC_ABC_SAFE_CALL_END();
}

//-*****************************************************************************
void OCurvesSchema::setTimeSampling( AbcA::TimeSamplingPtr iTime )
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN(
        "OCurvesSchema::setTimeSampling( TimeSamplingPtr )" );

    if ( iTime )
    {
        uint32_t tsIndex = getObject().getArchive().addTimeSampling( *iTime );
        setTimeSampling( tsIndex );
    }

    ALEMBIC_ABC_SAFE_CALL_END();
}

//-*****************************************************************************
void OCurvesSchema::init( const AbcA::index_t iTsIdx )
{
    ALEMBIC_ABC_SAFE_CALL_BEGIN( "OCurvesSchema::init()" );

    AbcA::MetaData mdata;
    SetGeometryScope( mdata, kVertexScope );

    AbcA::CompoundPropertyWriterPtr _this = this->getPtr();

    m_positionsProperty = Abc::OP3fArrayProperty( _this, "P", mdata, iTsIdx );

    m_nVerticesProperty = Abc::OInt32ArrayProperty( _this, "nVertices", iTsIdx);

    m_basisAndTypeProperty = Abc::OScalarProperty(
        _this, "curveBasisAndType",
        AbcA::DataType( Alembic::Util::kUint8POD, 4 ), iTsIdx );

    ALEMBIC_ABC_SAFE_CALL_END_RESET();
}

} // End namespace ALEMBIC_VERSION_NS
} // End namespace AbcGeom
} // End namespace Alembic