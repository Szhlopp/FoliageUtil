Shader "FoliageUtil/Foliage"
{
    Properties
    {
        _BaseMap ("Base color and coverage", 2D) = "white" {}
        _NormalMap ("OpenGL normal", 2D) = "bump" {}
        _PackedMap ("G roughness, B metallic", 2D) = "white" {}
        _BaseFactor ("Linear base factor", Vector) = (1,1,1,1)
        _Roughness ("Roughness", Range(0,1)) = 1
        _Metallic ("Metallic", Range(0,1)) = 0
        _Cutoff ("Alpha cutoff", Range(0,1)) = 0.5
        _Translucency ("Thin tissue transmission", Range(0,1)) = 0
        _TranslucencyColor ("Linear transmission tint", Vector) = (1,1,1,1)
        [HideInInspector] _AlphaMode ("Alpha mode", Float) = 0
        [HideInInspector] _HasNormal ("Normal map enabled", Float) = 0
        [HideInInspector] _Cull ("Cull", Float) = 0
        [HideInInspector] _SrcBlend ("Source blend", Float) = 1
        [HideInInspector] _DstBlend ("Destination blend", Float) = 0
        [HideInInspector] _ZWrite ("Depth write", Float) = 1
    }
    SubShader
    {
        PackageRequirements { "com.unity.render-pipelines.universal": "14.0" }
        Tags { "RenderPipeline"="UniversalPipeline" "RenderType"="Opaque" }
        Cull [_Cull]
        HLSLINCLUDE
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"
        TEXTURE2D(_BaseMap); SAMPLER(sampler_BaseMap);
        TEXTURE2D(_NormalMap); SAMPLER(sampler_NormalMap);
        TEXTURE2D(_PackedMap); SAMPLER(sampler_PackedMap);
        CBUFFER_START(UnityPerMaterial)
        float4 _BaseFactor, _TranslucencyColor;
        float _Roughness, _Metallic, _Cutoff, _Translucency, _AlphaMode, _HasNormal;
        CBUFFER_END
        struct Attributes { float4 positionOS:POSITION; float3 normalOS:NORMAL; float4 tangentOS:TANGENT; float4 color:COLOR; float2 uv:TEXCOORD0; };
        struct Varyings { float4 positionCS:SV_POSITION; float3 positionWS:TEXCOORD0; float3 normalWS:TEXCOORD1; float4 tangentWS:TEXCOORD2; float2 uv:TEXCOORD3; float4 color:COLOR; };
        Varyings vert(Attributes input)
        {
            Varyings output;
            output.positionWS=TransformObjectToWorld(input.positionOS.xyz);
            output.positionCS=TransformWorldToHClip(output.positionWS);
            output.normalWS=TransformObjectToWorldNormal(input.normalOS);
            output.tangentWS=float4(TransformObjectToWorldDir(input.tangentOS.xyz),input.tangentOS.w*GetOddNegativeScale());
            output.uv=input.uv; output.color=input.color;
            return output;
        }
        float4 pigment(float2 uv, float4 color)
        {
            float4 value=SAMPLE_TEXTURE2D(_BaseMap,sampler_BaseMap,uv)*_BaseFactor*color;
            if(_AlphaMode>0.5&&_AlphaMode<1.5) clip(value.a-_Cutoff);
            if(_AlphaMode<1.5) value.a=1;
            return value;
        }
        ENDHLSL
        Pass
        {
            Tags { "LightMode"="UniversalForward" }
            Blend [_SrcBlend] [_DstBlend]
            ZWrite [_ZWrite]
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile _ _MAIN_LIGHT_SHADOWS _MAIN_LIGHT_SHADOWS_CASCADE _MAIN_LIGHT_SHADOWS_SCREEN
            #pragma multi_compile _ _ADDITIONAL_LIGHTS
            #pragma multi_compile_fragment _ _ADDITIONAL_LIGHT_SHADOWS
            #pragma multi_compile_fragment _ _SHADOWS_SOFT
            #pragma multi_compile _ _FORWARD_PLUS
            half4 frag(Varyings input, FRONT_FACE_TYPE facing:FRONT_FACE_SEMANTIC):SV_Target
            {
                float4 color=pigment(input.uv,input.color);
                float3 normal=normalize(input.normalWS);
                if(_HasNormal>0.5)
                {
                    float3 tangent=normalize(input.tangentWS.xyz);
                    float3 bitangent=cross(normal,tangent)*input.tangentWS.w;
                    float3 mapped=normalize(SAMPLE_TEXTURE2D(_NormalMap,sampler_NormalMap,input.uv).xyz*2-1);
                    normal=normalize(tangent*mapped.x+bitangent*mapped.y+normal*mapped.z);
                }
                normal*=IS_FRONT_VFACE(facing,1,-1);
                float3 packed=SAMPLE_TEXTURE2D(_PackedMap,sampler_PackedMap,input.uv).rgb;
                InputData data=(InputData)0;
                data.positionWS=input.positionWS; data.normalWS=normal; data.viewDirectionWS=GetWorldSpaceNormalizeViewDir(input.positionWS);
                data.shadowCoord=TransformWorldToShadowCoord(input.positionWS); data.bakedGI=SampleSH(normal);
                data.normalizedScreenSpaceUV=GetNormalizedScreenSpaceUV(input.positionCS); data.shadowMask=half4(1,1,1,1);
                SurfaceData surface=(SurfaceData)0;
                surface.albedo=color.rgb; surface.alpha=color.a; surface.metallic=packed.b*_Metallic; surface.smoothness=1-packed.g*_Roughness; surface.occlusion=1;
                Light light=GetMainLight(data.shadowCoord);
                surface.emission=color.rgb*_TranslucencyColor.rgb*_Translucency*light.color*saturate(dot(-normal,light.direction))*light.distanceAttenuation*light.shadowAttenuation;
                return UniversalFragmentPBR(data,surface);
            }
            ENDHLSL
        }
        Pass
        {
            Tags { "LightMode"="ShadowCaster" }
            ZWrite On ZTest LEqual ColorMask 0
            HLSLPROGRAM
            #pragma vertex shadowVert
            #pragma fragment shadowFrag
            #pragma multi_compile_vertex _ _CASTING_PUNCTUAL_LIGHT_SHADOW
            float3 _LightDirection, _LightPosition;
            Varyings shadowVert(Attributes input)
            {
                Varyings output=vert(input);
                float3 direction=_LightDirection;
                #if defined(_CASTING_PUNCTUAL_LIGHT_SHADOW)
                direction=normalize(_LightPosition-output.positionWS);
                #endif
                output.positionCS=TransformWorldToHClip(ApplyShadowBias(output.positionWS,output.normalWS,direction));
                #if UNITY_REVERSED_Z
                output.positionCS.z=min(output.positionCS.z,UNITY_NEAR_CLIP_VALUE);
                #else
                output.positionCS.z=max(output.positionCS.z,UNITY_NEAR_CLIP_VALUE);
                #endif
                return output;
            }
            half4 shadowFrag(Varyings input):SV_Target
            {
                if(_AlphaMode>0.5) clip(SAMPLE_TEXTURE2D(_BaseMap,sampler_BaseMap,input.uv).a*_BaseFactor.a*input.color.a-_Cutoff);
                return 0;
            }
            ENDHLSL
        }
    }
    SubShader
    {
        Tags { "RenderType"="Opaque" }
        Cull [_Cull]
        CGINCLUDE
        #include "UnityCG.cginc"
        #include "Lighting.cginc"
        #include "AutoLight.cginc"
        #include "UnityPBSLighting.cginc"
        sampler2D _BaseMap, _NormalMap, _PackedMap;
        float4 _BaseFactor, _TranslucencyColor;
        float _Roughness, _Metallic, _Cutoff, _Translucency, _AlphaMode, _HasNormal;
        struct appdata { float4 vertex:POSITION; float3 normal:NORMAL; float4 tangent:TANGENT; float4 color:COLOR; float2 uv:TEXCOORD0; };
        ENDCG
        Pass
        {
            Tags { "LightMode"="ForwardBase" }
            Blend [_SrcBlend] [_DstBlend]
            ZWrite [_ZWrite]
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile_fwdbase
        struct v2f { float4 pos:SV_POSITION; float3 worldPos:TEXCOORD0; float3 normal:TEXCOORD1; float4 tangent:TEXCOORD2; float2 uv:TEXCOORD3; float4 color:COLOR; SHADOW_COORDS(4) };
        v2f vert(appdata v)
        {
            v2f o; o.pos=UnityObjectToClipPos(v.vertex); o.worldPos=mul(unity_ObjectToWorld,v.vertex).xyz;
            o.normal=UnityObjectToWorldNormal(v.normal); o.tangent=float4(UnityObjectToWorldDir(v.tangent.xyz),v.tangent.w*unity_WorldTransformParams.w);
            o.uv=v.uv; o.color=v.color; TRANSFER_SHADOW(o); return o;
        }
            float4 frag(v2f i, float facing:VFACE):SV_Target
            {
                float4 color=tex2D(_BaseMap,i.uv)*_BaseFactor*i.color;
                if(_AlphaMode>0.5&&_AlphaMode<1.5) clip(color.a-_Cutoff);
                if(_AlphaMode<1.5) color.a=1;
                float3 normal=normalize(i.normal);
                if(_HasNormal>0.5)
                {
                    float3 mapped=normalize(tex2D(_NormalMap,i.uv).xyz*2-1);
                    float3 tangent=normalize(i.tangent.xyz);
                    normal=normalize(tangent*mapped.x+cross(normal,tangent)*i.tangent.w*mapped.y+normal*mapped.z);
                }
                normal*=facing>0?1:-1;
                float3 packed=tex2D(_PackedMap,i.uv).rgb;
                UNITY_LIGHT_ATTENUATION(atten,i,i.worldPos);
                UnityLight light; light.color=_LightColor0.rgb*atten; light.dir=normalize(UnityWorldSpaceLightDir(i.worldPos)); light.ndotl=saturate(dot(normal,light.dir));
                UnityIndirect indirect; indirect.diffuse=ShadeSH9(float4(normal,1)); indirect.specular=0;
                float3 specular; float reflectivity;
                float3 diffuse=DiffuseAndSpecularFromMetallic(color.rgb,packed.b*_Metallic,specular,reflectivity);
                float4 result=UNITY_BRDF_PBS(diffuse,specular,reflectivity,1-packed.g*_Roughness,normal,normalize(UnityWorldSpaceViewDir(i.worldPos)),light,indirect);
                result.rgb+=color.rgb*_TranslucencyColor.rgb*_Translucency*light.color*saturate(dot(-normal,light.dir)); result.a=color.a;
                return result;
            }
            ENDCG
        }
        Pass
        {
            Tags { "LightMode"="ShadowCaster" }
            ZWrite On ZTest LEqual
            CGPROGRAM
            #pragma vertex shadowVert
            #pragma fragment shadowFrag
            #pragma multi_compile_shadowcaster
            struct shadowData { V2F_SHADOW_CASTER; float2 uv:TEXCOORD1; float alpha:TEXCOORD2; };
            shadowData shadowVert(appdata v) { shadowData o; TRANSFER_SHADOW_CASTER_NORMALOFFSET(o); o.uv=v.uv; o.alpha=v.color.a; return o; }
            float4 shadowFrag(shadowData i):SV_Target { if(_AlphaMode>0.5) clip(tex2D(_BaseMap,i.uv).a*_BaseFactor.a*i.alpha-_Cutoff); SHADOW_CASTER_FRAGMENT(i); }
            ENDCG
        }
    }
    Fallback Off
}
