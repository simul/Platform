

vec4 quat_from_axis_angle_degrees(vec3 axis, float angle)
{ 
  vec4 qr;
  float half_angle = (angle * 0.5) * 3.14159 / 180.0;
  qr.x = axis.x * sin(half_angle);
  qr.y = axis.y * sin(half_angle);
  qr.z = axis.z * sin(half_angle);
  qr.w = cos(half_angle);
  return qr;
}

vec4 quat_from_axis_angle_radians(vec3 axis, float angle)
{
	vec4 qr;
	float half_angle = angle * 0.5;
	qr.x = axis.x * sin(half_angle);
	qr.y = axis.y * sin(half_angle);
	qr.z = axis.z * sin(half_angle);
	qr.w = cos(half_angle);
	return qr;
}

vec4 quat_conj(vec4 q)
{ 
  return vec4(-q.x, -q.y, -q.z, q.w); 
}

vec4 quat_inverse(vec4 q)
{ 
	float len = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
	if (len == 0.0f)
	{
		// Invalid argument
		return vec4(0, 0, 0, 0);
	}
	float invLen = 1.0f / len;
	return vec4(-q.x * invLen, -q.y * invLen, -q.z * invLen, q.w * invLen);
}
  
vec4 quat_mult(vec4 q1, vec4 q2)
{ 
  vec4 qr;
  qr.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
  qr.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
  qr.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
  qr.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
  return qr;
}

vec4 quat_vec(vec4 q, vec3 v)
{ 
	vec4 qr;
	qr.w =-(q.x * v.x) - (q.y * v.y) - (q.z * v.z);
	qr.x = (q.w * v.x) + (q.y * v.z) - (q.z * v.y);
	qr.y = (q.w * v.y) + (q.z * v.x) - (q.x * v.z);
	qr.z = (q.w * v.z) + (q.x * v.y) - (q.y * v.x);
	return qr;
}

vec3 rotate_by_quaternion(vec4 quat, vec3 position)
{ 
  vec4 qr_conj		= quat_conj(quat);
  vec4 q_tmp		= quat_vec(quat,position);
  vec4 qr			= quat_mult(q_tmp,qr_conj);
  return qr.xyz;
}

vec3 rotate_by_inverse_quaternion(vec4 quat, vec3 position)
{
	vec4 qr_conj = quat_conj(quat);
	vec4 q_tmp = quat_vec(qr_conj, position);
	vec4 qr = quat_mult(q_tmp, quat);
	return qr.xyz;
}

vec4 slerp(vec4 q1,vec4 q2,float interp)
{
	vec4 Q1 = normalize(q1);
	vec4 Q2 = normalize(q2);

	float dot = Q1.x*Q2.x + Q1.y*Q2.y + Q1.z*Q2.z + Q1.w*Q2.w;
	if(dot < 0.0)
	{
		dot=-dot;
		Q2.x *= -1.0;
		Q2.y *= -1.0;
		Q2.z *= -1.0;
		Q2.w *= -1.0;
	}
	const float DOT_THRESHOLD = 0.9995;
	if (dot > DOT_THRESHOLD)
	{
		// If the inputs are too close for comfort, linearly interpolate
		// and normalize the result.
		vec4 ret;
		ret.x = Q1.x + interp * (Q2.x - Q1.x);
		ret.y = Q1.y + interp * (Q2.y - Q1.y);
		ret.z = Q1.z + interp * (Q2.z - Q1.z);
		ret.w = Q1.w + interp * (Q2.w - Q1.w);
		ret		=normalize(ret);
		return ret;
	}
	if (dot < -1.0)
		dot = -1.0;
	if (dot > 1.0)
		dot = 1.0;

	float theta_0 = acos(dot);  // theta_0 = half angle between input vectors
	float theta1 = theta_0 * (1.0 - interp);    // theta = angle between v0 and result 
	float theta2 = theta_0 * interp;    // theta = angle between v0 and result 

	float s1 = sin(theta1);
	float s2 = sin(theta2);
	float ss = sin(theta_0);

	vec4 ret;
	ret.x = (Q1.x * s1 + Q2.x * s2) / ss;
	ret.y = (Q1.y * s1 + Q2.y * s2) / ss;
	ret.z = (Q1.z * s1 + Q2.z * s2) / ss;
	ret.w = (Q1.w * s1 + Q2.w * s2) / ss;
	ret = normalize(ret);
	return ret;
}