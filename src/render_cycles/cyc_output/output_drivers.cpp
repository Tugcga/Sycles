#include "output_drivers.h"
#include "../../utilities/logs.h"

XSIOutputDriver::XSIOutputDriver(RenderEngineCyc* cycles_render)
{
    m_cycles_render = cycles_render;
}

XSIOutputDriver::~XSIOutputDriver()
{
}

// this callback called several times during the render process
bool XSIOutputDriver::update_render_tile(const Tile& tile)
{
    m_cycles_render->update_render_tile(tile);
    return true;
}

// this callback calls only once when the final image (or part) is rendered
void XSIOutputDriver::write_render_tile(const Tile& tile)
{
    
}

bool XSIOutputDriver::read_render_tile(const Tile& tile)
{
    m_cycles_render->read_render_tile(tile);
    return true;
}