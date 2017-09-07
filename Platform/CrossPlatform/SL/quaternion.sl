

vec4 quat_from_axis_angle(vec3 axis, float angle)
{ 
  vec4 qr;
  float half_angle = (angle * 0.5) * 3.14159 / 180.0;
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
//	quat.xyz*=-1.0;
  vec4 qr_conj		= quat_conj(quat);
  vec4 q_pos		= vec4(position, 0);
  
  vec4 q_tmp		= quat_vec(quat,position);
  vec4 qr			= quat_mult(q_tmp,qr_conj);
  
  return qr.xyz;
}