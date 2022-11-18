#pragma once

#include "session/output_driver.h"

#include "../../render_cycles/render_engine_cyc.h"

class XSIOutputDriver : public ccl::OutputDriver 
{
public:
    XSIOutputDriver(RenderEngineCyc* cycles_render);
    virtual ~XSIOutputDriver();

    virtual void write_render_tile(const Tile& tile) override;
    virtual bool read_render_tile(const Tile& tile) override;
    virtual bool update_render_tile(const Tile& tile) override;
private:
    RenderEngineCyc* m_cycles_render;
};