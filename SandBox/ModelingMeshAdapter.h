#ifndef MODELINGMESHADAPTER_H
#define MODELINGMESHADAPTER_H

#include "ModelingMesh.h"

#include "Resource/ExternalMeshDataSource.h"

/// ModelingMesh 到 MyOpenGL ExternalMeshDataSource 的零中间 Mesh 拷贝 Adapter。
class ModelingMeshAdapter : public ExternalMeshDataSource
{
public:
    explicit ModelingMeshAdapter(const ModelingMesh* mesh = 0);

    /// Source
    void setMesh(const ModelingMesh* mesh);
    const ModelingMesh* mesh() const;

    /// ExternalMeshDataSource
    bool dataView(ExternalMeshDataView& view) const override;
    ExternalMeshRevision structureRevision() const override;
    ExternalMeshRevision contentRevision() const override;
    bool changesSince(ExternalMeshRevision previousRevision, ExternalMeshChangeSet& changeSet) const override;

private:
    const ModelingMesh* m_mesh; // 不拥有外部建模 Mesh。
};

#endif // MODELINGMESHADAPTER_H