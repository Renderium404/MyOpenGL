#ifndef MODELINGMESHADAPTER_H
#define MODELINGMESHADAPTER_H

#include "ModelingMesh.h"

#include "Resource/ExternalGeometryDataSource.h"

/// ModelingMesh 到 MyOpenGL ExternalGeometryDataSource 的零中间 Geometry 拷贝 Adapter。
class ModelingMeshAdapter : public ExternalGeometryDataSource
{
public:
    explicit ModelingMeshAdapter(const ModelingMesh* mesh = 0);

    /// Source
    void setMesh(const ModelingMesh* mesh);
    const ModelingMesh* mesh() const;

    /// ExternalGeometryDataSource
    bool dataView(ExternalGeometryDataView& view) const override;
    ExternalGeometryRevision structureRevision() const override;
    ExternalGeometryRevision contentRevision() const override;
    bool changesSince(ExternalGeometryRevision previousRevision, ExternalGeometryChangeSet& changeSet) const override;

private:
    const ModelingMesh* m_mesh; // 不拥有外部建模 Mesh。
};

#endif // MODELINGMESHADAPTER_H