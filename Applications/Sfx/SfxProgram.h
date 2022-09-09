#ifndef PSFXPROGRAM_H
#define PSFXPROGRAM_H

namespace sfx
{
	class Pass
	{
	public:
		PassState passState;
		Pass(const char *n);
		Pass(const char *n,const PassState &passState);
		bool HasShader(ShaderType t) const
		{
			return m_shaderInstanceNames[(int)t].length()>0;
		}
		std::string GetShader(ShaderType t) const
		{
			return m_shaderInstanceNames[(int)t];
		}
		std::string name;
	private:
		std::string m_shaderInstanceNames[NUM_OF_SHADER_COMMANDS];
		friend int ::sfxparse();
	};
	class Technique
	{
	public:
		Technique(const std::vector<Pass>& passes);
		const std::vector< Pass>	&GetPasses()  const
		{
			return passes;
		}
		const std::map<std::string, Pass*>	&GetPassMap()  const
		{
			return m_passes;
		}
	private:
		std::map<std::string, Pass*>	m_passes;
		std::vector<Pass>	passes;
		friend int ::sfxparse();
	};
}
#endif