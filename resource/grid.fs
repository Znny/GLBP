#version 330

in vec2 ndc;
out vec4 frag_colour;

uniform mat4 InverseViewProjectionMatrix;
uniform mat4 ViewProjectionMatrix; // forward matrix, needed to recompute gl_FragDepth below
uniform vec3 CameraWorldPosition;
uniform vec3 PlaneTangent;   // orthonormal, spans the grid plane together with PlaneBitangent
uniform vec3 PlaneBitangent;
uniform vec3 TangentAxisColor;
uniform vec3 BitangentAxisColor;

const float MinorSpacing = 1.0;
const float MajorSpacing = 10.0;
const float FadeStartDistance = 40.0;
const float FadeEndDistance = 100.0;

vec3 UnprojectNDC(vec2 XY, float Z)
{
    vec4 World = InverseViewProjectionMatrix * vec4(XY, Z, 1.0);
    return World.xyz / World.w;
}

// 1.0 exactly on a grid line of the given Spacing, falling off to 0.0 within ~1px (fwidth-based
// analytic antialiasing - no supersampling needed since the line pattern is defined algebraically).
float GridLine(vec2 Coord, float Spacing)
{
    vec2 ScaledCoord = Coord / Spacing;
    vec2 Deriv = fwidth(ScaledCoord);
    vec2 Grid = abs(fract(ScaledCoord - 0.5) - 0.5) / max(Deriv, vec2(1e-6));
    return 1.0 - min(min(Grid.x, Grid.y), 1.0);
}

void main()
{
    vec3 Normal = normalize(cross(PlaneTangent, PlaneBitangent));

    vec3 NearPoint = UnprojectNDC(ndc, -1.0);
    vec3 FarPoint = UnprojectNDC(ndc, 1.0);
    vec3 RayDelta = FarPoint - NearPoint;

    float Denominator = dot(RayDelta, Normal);
    if(abs(Denominator) < 1e-6)
    {
        discard; // ray runs parallel to the plane, never intersects it
    }

    float T = -dot(NearPoint, Normal) / Denominator;
    if(T < 0.0 || T > 1.0)
    {
        discard; // plane intersection falls outside the near/far clip range for this pixel
    }

    vec3 WorldPos = NearPoint + T * RayDelta;
    vec2 PlaneCoord = vec2(dot(WorldPos, PlaneTangent), dot(WorldPos, PlaneBitangent));

    float MinorLine = GridLine(PlaneCoord, MinorSpacing);
    float MajorLine = GridLine(PlaneCoord, MajorSpacing);
    float LineAlpha = max(MinorLine * 0.35, MajorLine * 0.7);

    vec3 Color = vec3(0.5);

    float BitangentAxisWidth = fwidth(PlaneCoord.x) * 1.5;
    if(abs(PlaneCoord.x) < BitangentAxisWidth)
    {
        Color = BitangentAxisColor;
        LineAlpha = max(LineAlpha, 0.9);
    }

    float TangentAxisWidth = fwidth(PlaneCoord.y) * 1.5;
    if(abs(PlaneCoord.y) < TangentAxisWidth)
    {
        Color = TangentAxisColor;
        LineAlpha = max(LineAlpha, 0.9);
    }

    float DistanceToCamera = length(WorldPos - CameraWorldPosition);
    float DistanceFade = 1.0 - smoothstep(FadeStartDistance, FadeEndDistance, DistanceToCamera);

    if(LineAlpha * DistanceFade <= 0.0)
    {
        discard;
    }

    frag_colour = vec4(Color, LineAlpha * DistanceFade);

    // T is linear in world space, but NDC depth isn't linear in world-space ray distance under a
    // perspective projection - so T can't be used as gl_FragDepth directly (it would only be
    // correct for an orthographic camera). Reproject WorldPos through the real forward matrix instead.
    vec4 ClipPos = ViewProjectionMatrix * vec4(WorldPos, 1.0);
    float NdcDepth = ClipPos.z / ClipPos.w;
    gl_FragDepth = NdcDepth * 0.5 + 0.5;
};
