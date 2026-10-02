#pragma once
#include <imgui.h>

/**
 * The headphone illustration, in real-time 3D. A real model is shown when Models/ next to the
 * executable holds one for the product (converted from a 3D file with tooling/ConvertModel.py);
 * otherwise a model generated in code (our own design) stands in. A view is rendered in software
 * into a texture, lit like a studio product shot in the product's own colour, and rendered again
 * only when the view changes.
 */
namespace Headphones3D
{
    struct View
    {
        float yaw = 0.62f;  // Radians. 0 faces the viewer front on; positive turns the right cup away
        float pitch = 0.2f; // Radians. Positive looks down onto the headband
        float size = 64.0f; // Half the model's height on screen, in pixels
    };

    /**
     * Draw the model for @p product centred on @p center.
     * @param product A model name ("WH-1000XM5"), or a device name that contains one. Empty for
     *                headphones in general.
     * @param colour  The product colour.
     * @param alpha   Fades the whole model. It is a single flat image, so nothing shows through.
     */
    void Draw(ImDrawList* draw, ImVec2 center, const View& view, const char* product, ImU32 colour,
              float alpha = 1.0f);

    /** Width over height of the model's bounding box, to lay it out. */
    float Aspect(const char* product);

    /** The yaw a model looks best at: a three-quarter view for headphones, side by side for earbuds. */
    float RestYaw(const char* product);
}
