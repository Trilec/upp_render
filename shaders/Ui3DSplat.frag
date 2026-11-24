#version 450
layout(location=0) in vec2 gaussian;
layout(location=1) in vec4 color;
layout(location=2) flat in vec4 highlight;
layout(location=0) out vec4 output_color;
void main()
{
    float squared=dot(gaussian,gaussian);
    float alpha=color.a*exp(-0.5*squared);
    if(squared>11.082527090316852 || alpha<1.0/255.0) discard;
    // Selection is a view decoration over the actual ellipse, not source edits.
    float radius=sqrt(squared);
    float ring=smoothstep(1.8,2.2,radius)*(1.0-smoothstep(2.8,3.2,radius));
    vec3 rgb=mix(color.rgb,highlight.rgb,highlight.a*ring);
    output_color=vec4(rgb,alpha);
}
