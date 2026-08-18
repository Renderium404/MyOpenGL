#ifndef EXTERNALGEOMETRYDATASOURCE_H
#define EXTERNALGEOMETRYDATASOURCE_H

#include "ExternalGeometryData.h"

/// 外部 Geometry 的数据源接口。
/// MyOpenGL 不拥有 DataSource；外部建模库或其他几何库通常通过 Application Adapter 实现该接口。
class ExternalGeometryDataSource
{
public:
    virtual ~ExternalGeometryDataSource()
    {
    }

    /// 当前完整数据视图
    virtual bool dataView(ExternalGeometryDataView& view) const = 0;

    /// Revision
    virtual ExternalGeometryRevision structureRevision() const = 0; // DataView 地址、大小、Layout 或拓扑变化时递增。
    virtual ExternalGeometryRevision contentRevision() const = 0;   // 当前 DataView 内已有字节内容发生变化时递增。

    /// 增量变化
    virtual bool changesSince(ExternalGeometryRevision previousRevision, ExternalGeometryChangeSet& changeSet) const = 0; // 无法提供完整历史时返回 false，Resource 将退化为 Full Update。
};

#endif // EXTERNALGEOMETRYDATASOURCE_H
