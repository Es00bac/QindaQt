// SPDX-License-Identifier: GPL-3.0-or-later

#include "xcb_tray_visual.h"

#include <cstdlib>

namespace QindaQt::XEmbedTray
{

TrayVisual pickTrayVisual(xcb_connection_t *connection, xcb_screen_t *screen)
{
    TrayVisual result;
    result.visualId = screen->root_visual;
    result.depth = screen->root_depth;
    result.colormap = screen->default_colormap;
    result.blackPixel = screen->black_pixel;

    xcb_depth_iterator_t depthIterator = xcb_screen_allowed_depths_iterator(screen);
    xcb_depth_t *argbDepth = nullptr;
    while (depthIterator.rem) {
        if (depthIterator.data->depth == 32) {
            argbDepth = depthIterator.data;
            break;
        }
        xcb_depth_next(&depthIterator);
    }
    if (argbDepth == nullptr) {
        return result;
    }

    xcb_visualtype_iterator_t visualIterator = xcb_depth_visuals_iterator(argbDepth);
    xcb_visualid_t argbVisual = 0;
    while (visualIterator.rem) {
        if (visualIterator.data->_class == XCB_VISUAL_CLASS_TRUE_COLOR) {
            argbVisual = visualIterator.data->visual_id;
            break;
        }
        xcb_visualtype_next(&visualIterator);
    }
    if (argbVisual == 0 || argbVisual == screen->root_visual) {
        return result;
    }

    const xcb_colormap_t colormap = xcb_generate_id(connection);
    xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap,
                        screen->root, argbVisual);
    xcb_alloc_color_cookie_t colorCookie =
        xcb_alloc_color(connection, colormap, 0, 0, 0);
    xcb_alloc_color_reply_t *colorReply =
        xcb_alloc_color_reply(connection, colorCookie, nullptr);
    if (colorReply != nullptr) {
        result.blackPixel = colorReply->pixel;
        free(colorReply);
    } else {
        result.blackPixel = 0;
    }

    result.visualId = argbVisual;
    result.depth = 32;
    result.colormap = colormap;
    return result;
}

void freeTrayVisual(xcb_connection_t *connection, xcb_screen_t *screen,
                    TrayVisual *visual)
{
    if (visual->colormap != screen->default_colormap) {
        xcb_free_colormap(connection, visual->colormap);
    }
}

} // namespace QindaQt::XEmbedTray
