#include "SSTextRenderer.h"

#include <cstdio>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "myc/paths/paths.h"

SSTextRenderer::~SSTextRenderer()
{
    if (AtlasTexture != 0)
    {
        glDeleteTextures(1, &AtlasTexture);
    }
    if (VertexBuffer != 0)
    {
        glDeleteBuffers(1, &VertexBuffer);
    }
    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
    }
}

bool SSTextRenderer::Initialize(const std::string& TTFFilename, float FontPixelHeight)
{
    const std::string FullPath = myc::GetExecutableDir() + TTFFilename;

    FILE* File = fopen(FullPath.c_str(), "rb");
    if (!File)
    {
        return false;
    }

    fseek(File, 0, SEEK_END);
    const long FileSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    std::vector<unsigned char> FontBuffer(FileSize);
    fread(FontBuffer.data(), 1, FileSize, File);
    fclose(File);

    // ASCII-only fixed-size atlas: simple and enough for a basic renderer, at the cost of not
    // handling glyphs that don't fit or non-ASCII text.
    std::vector<unsigned char> AtlasBitmap(AtlasWidth * AtlasHeight);
    stbtt_BakeFontBitmap(FontBuffer.data(), 0, FontPixelHeight,
                          AtlasBitmap.data(), AtlasWidth, AtlasHeight,
                          FirstChar, NumChars, CharData);

    glGenTextures(1, &AtlasTexture);
    glBindTexture(GL_TEXTURE_2D, AtlasTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, AtlasWidth, AtlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, AtlasBitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    Shader = Rendering::ShaderManager::Get()->LoadShaderProgram("sstext", "/resource/text.vs", "/resource/text.fs");

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);

    // interleaved vec2 position + vec2 uv, re-uploaded per DrawText call
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    bInitialized = (Shader != nullptr);
    return bInitialized;
}

void SSTextRenderer::SetScreenSize(int Width, int Height)
{
    ScreenWidth = Width;
    ScreenHeight = Height;
}

float SSTextRenderer::MeasureTextWidth(const std::string& Text, float Scale) const
{
    float Width = 0.0f;
    for (const char Character : Text)
    {
        if (Character < FirstChar || Character >= FirstChar + NumChars)
        {
            continue;
        }
        Width += CharData[Character - FirstChar].xadvance;
    }
    return Width * Scale;
}

void SSTextRenderer::DrawText(const std::string& Text, float X, float Y, const glm::vec3& Color, float Scale) const
{
    if (!bInitialized)
    {
        return;
    }

    std::vector<float> Vertices;
    Vertices.reserve(Text.size() * 6 * 4);

    float PenX = X;
    float PenY = Y;

    for (const char Character : Text)
    {
        if (Character < FirstChar || Character >= FirstChar + NumChars)
        {
            continue;
        }

        const float AnchorX = PenX;
        const float AnchorY = PenY;

        stbtt_aligned_quad Quad;
        stbtt_GetBakedQuad(CharData, AtlasWidth, AtlasHeight, Character - FirstChar, &PenX, &PenY, &Quad, 1);

        // rescale the glyph quad and the pen advance around the pre-glyph anchor point
        const float X0 = AnchorX + (Quad.x0 - AnchorX) * Scale;
        const float X1 = AnchorX + (Quad.x1 - AnchorX) * Scale;
        const float Y0 = AnchorY + (Quad.y0 - AnchorY) * Scale;
        const float Y1 = AnchorY + (Quad.y1 - AnchorY) * Scale;
        PenX = AnchorX + (PenX - AnchorX) * Scale;
        PenY = AnchorY + (PenY - AnchorY) * Scale;

        const float GlyphVerts[] =
        {
            X0, Y0, Quad.s0, Quad.t0,
            X0, Y1, Quad.s0, Quad.t1,
            X1, Y1, Quad.s1, Quad.t1,

            X0, Y0, Quad.s0, Quad.t0,
            X1, Y1, Quad.s1, Quad.t1,
            X1, Y0, Quad.s1, Quad.t0,
        };
        Vertices.insert(Vertices.end(), std::begin(GlyphVerts), std::end(GlyphVerts));
    }

    if (Vertices.empty())
    {
        return;
    }

    const glm::mat4 ProjectionMatrix = glm::ortho(0.0f, (float)ScreenWidth, (float)ScreenHeight, 0.0f, -1.0f, 1.0f);

    const GLuint ProgramID = Shader->GetProgramID();
    glUseProgram(ProgramID);
    glUniformMatrix4fv(glGetUniformLocation(ProgramID, "ProjectionMatrix"), 1, GL_FALSE, &ProjectionMatrix[0][0]);
    glUniform3fv(glGetUniformLocation(ProgramID, "TextColor"), 1, &Color[0]);
    glUniform1i(glGetUniformLocation(ProgramID, "FontAtlas"), 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, AtlasTexture);

    // text is a flat screen-space overlay: no depth test, and glyph edges need alpha blending
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(Vertices.size() * sizeof(float)), Vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(Vertices.size() / 4));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
