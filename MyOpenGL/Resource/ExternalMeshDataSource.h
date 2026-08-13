#ifndef EXTERNALMESHDATASOURCE_H
#define EXTERNALMESHDATASOURCE_H

#include "Resource/ExternalMeshData.h"

/// 外部建模网格的数据源接口。
/// MyOpenGL 不拥有 DataSource；真实建模库通常通过 Application 层 Adapter 实现该接口。
class ExternalMeshDataSource
{
public:
    virtual ~ExternalMeshDataSource()
    {
    }

    /// 当前完整数据视图
    virtual bool dataView(ExternalMeshDataView& view) const = 0;

    /// Revision
    virtual ExternalMeshRevision structureRevision() const = 0; // DataView 地址、大小、Layout 或拓扑变化时递增。
    virtual ExternalMeshRevision contentRevision() const = 0;   // 当前 DataView 内已有字节内容发生变化时递增。

    /// 增量变化
    virtual bool changesSince(ExternalMeshRevision previousRevision, ExternalMeshChangeSet& changeSet) const = 0; // 无法提供完整历史时返回 false，Renderer 将退化为 Full Update。
};

#endif // EXTERNALMESHDATASOURCE_H