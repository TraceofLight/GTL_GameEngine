struct PARTICLE_VS_INPUT
{
	/** The position of the particle. */
	float3 Position : POSITION;
	/** The relative time of the particle. */
	float RelativeTime : RELATIVE_TIME;
	/** The previous position of the particle. */
	float3 OldPosition : OLD_POSITION;
	/** Value that remains constant over the lifetime of a particle. */
	float ParticleId : PARTICLE_ID;
	/** The size of the particle. */
	float2 Size : SIZE;
	/** The rotation of the particle. */
	float Rotation : ROTATION;
	/** The sub-image index for the particle. */
	float SubImageIndex : SUB_IMAGE_INDEX;
	/** The color of the particle. */
	float4 Color : COLOR;

	float2 TexCoord : TEXCOORD0;
};

void GetTangents(row_major float4x4 InverseViewMatrix,
	float3 TranslatedWorldPosition, float3 OldTranslatedWorldPosition, float SpriteRotation,
	out float3 OutRight,
	out float3 OutUp)
{
	// Select camera up/right vectors.
	float3 CameraRight = InverseViewMatrix[1].xyz;
	float3 CameraUp = InverseViewMatrix[2].xyz;

	// Determine the vector from the particle to the camera and the particle's movement direction.
	float3 CameraDirection = normalize(InverseViewMatrix[3].xyz - TranslatedWorldPosition);
	//float3 RightVector = CameraRight.xyz;
	//float3 UpVector = CameraUp.xyz;
	
	// Tangent vectors for camera facing position.
	float3 RightVector = normalize(cross(CameraDirection, float3(0, 0, 1)));
	float3 UpVector = cross(RightVector, CameraDirection);

	// Determine the angle of rotation.
	float SinRotation; // = 0
	float CosRotation; // = 1
	sincos(SpriteRotation, SinRotation, CosRotation);

	// Rotate the sprite to determine final tangents.
	OutRight = SinRotation * UpVector + CosRotation * RightVector;
	OutUp = CosRotation * UpVector - SinRotation * RightVector;
}

void ComputeBillboardUVs(PARTICLE_VS_INPUT Input, out float2 UVForPosition, out float2 UVForTexturing, out float2 UVForTexturingUnflipped)
{
	// Encoding the UV flip in the sign of the size data.
	float2 UVFlip = Input.Size;

	UVForTexturing.x = UVFlip.x < 0.0 ? 1.0 - Input.TexCoord.x : Input.TexCoord.x;
	UVForTexturing.y = UVFlip.y < 0.0 ? 1.0 - Input.TexCoord.y : Input.TexCoord.y;

	// Note: not inverting positions, as that would change the winding order
	UVForPosition = UVForTexturingUnflipped = Input.TexCoord.xy;
}
