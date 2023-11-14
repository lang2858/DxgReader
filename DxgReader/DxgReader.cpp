// DxgReader.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <string>
#include <fstream>
#include <io.h>
#include<iostream>
#include <assert.h>
#include <direct.h>
#include<vector>
#include <sstream>
#include <iomanip>
#include "Inverse-matrix.h"
#include "mrb_file.h"
#include<set>

using namespace std;


void split(const string& s,vector<string>& sv,const char flag = ' ') {
	sv.clear();
	istringstream iss(s);
	string temp;

	while (getline(iss, temp, flag)) {
		sv.push_back(temp);
	}
	return;
}

struct file_header {
	char signature[4];//"DXG"
	int version; 
	int flag; //固定为7 按位使用，先不管
	int nr_groups; //组数
	int next_data_offset; // + 0x14(前面这5个int) = auxdata的起始位，就是从读完这个字段后的位置偏移咯
	int group_names_size;
	char group_names[256]; //[group_names_size];
};

struct group_header {
	int flag_b1;  //固定03 or 01
	int offset_f;
	int flag_b2;  //固定为0
	int flag_b3;
	int offset_g;
	short verteces_nr;
	short normals_nr;
	int uv_nr;
	int weight_map_nr;  //权重数，权重是：控制关节扭曲时,附近的顶点变形的扭曲程度

};

struct mesh_header {
	short vertex_info_nr; //顶点信息数 定义index的
	short face_info_nr;  //三角形绘制序列
	int name_t_nr; //名单数
	int data_z_nr;  //data z 数 ？？ unknown
	int verteces_offset;  //从 mesh_head 开始位置 偏移 verteces_offset个就是 vertex_coordinate ,验证对的
};

//顶点信息(不是定点坐标哦)
struct vertex_info { //个数为面的3倍
	short vertex_index;  // 定点索引
	short normal_index;// 法线索引
	short uv_index;  // uv索引
	short unknown1;
	//   short unknown2;   //没有这个字段
};

// 面 信息 
struct face_info {
	short vi[3];  //顶点vertex_info 信息索引（不是顶点坐标数组的索引）
};

struct z_info { //个数为顶点信息数的3倍 
	char* data; //[group_header.data_u_nr];
	char* padding;  //补位：data_u整体大小为4的倍数
};

struct mesh_data {
	struct mesh_header mesh_header;
	struct vertex_info* vertex_info;
	struct face_info* face_info;  //triangles 三角形绘制序列
	struct name_t* name_t;
	int names_size;
	char names[256];
	struct z_info z_info;
};

// 頂点座標
struct vertex_coordinate {
	float x, y, z;
};

struct normal_vector {
	float x, y, z;
};

struct uv_coordinate {
	float u, v;
};

struct weight_map {
	float w0, w1;
};

struct tf_data {
	short flag1; //0
	short count; // size/6
	int flag2; //0
	int flag3; //0
	int dataSize; //下面data的大小
	short* data;  //count * 3 个 short,3个一组
};

struct tris_info
{
	int flag1;  //0 
	short flag2; //0
	short count;
	int alldataSize;
	int flag3; //0
	int flag4; //0
	int flag5; //0
	struct tf_data* dat; 
};

struct group_data {
	struct group_header group_header;
	struct mesh_data* mesh_list;
	struct vertex_coordinate* verteces; // [group_header.verteces_nr];  == 1a0   6 * 12 = 72 (0x48)   "GM4_01_006.dxg"为例
	struct normal_vector* normals; // [group_header.normals_nr];  ==	1e8  8 	* 12 = 96 (0x60)	
	struct uv_coordinate* uvs; // [group_header.uv_nr];  == 248  5 * 8 = 40 (0x28)  
	struct weight_map* weight_map; // [group_header.data_v_nr] == 270    12 * 4 = 48 (0x30)

	struct tris_info tex_fs; //可能是这个，先取这个名字
	struct tris_info tri_fs;
};

//Auxiliary 骨骼数据  Skeletal/Bone

struct bone_link {  //关节
	char index;
	char parent; //父关节编号,FF为没有，属于根节点
	char child;
	char sibling;
	float v[16];  //matrix4x4
	string* name;
};

struct aux_data {
	//==header==
	int nr_aux_elems;
	int aux_data_size;
	//==
	int aux_names_size;
	char* aux_names; //空格隔开的
	bone_link* bone_links;
};

//后面还有点FX 粒子特效

//==
static void inOrderTraveral(ofstream& m_out_Stream, bone_link* bone_links, bone_link* p_bone_link, int  depth = 3)
{
	if(p_bone_link == NULL)
		return;
	else
	{
		for(int j=0;j< depth;j++)
			m_out_Stream << "\t";
		m_out_Stream <<"(transform \""<< p_bone_link->name->c_str() <<"\""<< endl;
		for(int j=0;j< depth + 1;j++)
			m_out_Stream << "\t";
		m_out_Stream <<"(matrix  ( ";  //matrix

		InverseMatrix matrix(p_bone_link->v);
		matrix.determinte = matrix.Determinte(matrix.cinMatrix, matrix.sizeMatrix); 
		matrix.Inverse();	  //坑爹呀，也要逆矩阵
		//再坑爹呀，逆完了还要翻转放置，不是横向按列，是纵向~~~
		m_out_Stream << "(" << matrix.coutMatrix[0][0] << " " <<  matrix.coutMatrix[1][0] << " " << matrix.coutMatrix[2][0] << " " << matrix.coutMatrix[3][0] <<") ";
		m_out_Stream << "(" << matrix.coutMatrix[0][1] << " " <<  matrix.coutMatrix[1][1] << " " << matrix.coutMatrix[2][1] << " " << matrix.coutMatrix[3][1] <<") ";
		m_out_Stream << "(" << matrix.coutMatrix[0][2] << " " <<  matrix.coutMatrix[1][2] << " " << matrix.coutMatrix[2][2] << " " << matrix.coutMatrix[3][2] <<") ";
		m_out_Stream << "(" << matrix.coutMatrix[0][3] << " " <<  matrix.coutMatrix[1][3] << " " << matrix.coutMatrix[2][3] << " " << matrix.coutMatrix[3][3] <<") ";
		//m_out_Stream << "(" << p_bone_link->v[0] << " " << p_bone_link->v[1] << " " << p_bone_link->v[2] << " " << p_bone_link->v[3] <<") ";
		//m_out_Stream << "(" << p_bone_link->v[4] << " " << p_bone_link->v[6] << " " << p_bone_link->v[6] << " " << p_bone_link->v[7] <<") ";
		//m_out_Stream << "(" << p_bone_link->v[8] << " " << p_bone_link->v[9] << " " << p_bone_link->v[10] << " " << p_bone_link->v[11] <<") ";
		//m_out_Stream << "(" << p_bone_link->v[12] << " " << p_bone_link->v[13] << " " << p_bone_link->v[14] << " " << p_bone_link->v[15] <<") ";
		m_out_Stream << "))" << endl; //matrix

		if(p_bone_link->child != -1) 
		{ 
			for(int j=0;j<depth+1;j++)
				m_out_Stream << "\t";
			m_out_Stream << "(children (" << endl;

			inOrderTraveral(m_out_Stream, bone_links, &(bone_links[p_bone_link->child]), depth + 2);

			for(int j=0;j< depth + 1;j++)
				m_out_Stream << "\t";
			m_out_Stream << "))" <<endl; //children
		} 
		for(int j=0;j< depth;j++)
			m_out_Stream << "\t";
		m_out_Stream << ")" <<endl; //transform
	}
	if(p_bone_link->sibling != -1)
		inOrderTraveral(m_out_Stream, bone_links, &(bone_links[p_bone_link->sibling]),depth);
}

//========================================write lta=======================
// dxg3版本才骨骼动画这些
//deformer
struct v_bones
{
	set<int> bone_indexs;
};

void saveAsLTA(file_header& m_fileHeader, group_data* m_group_data, aux_data* p_aux_data, vector<string> &bone_names)
{
	const char* objPath = "..\\dxg.lta";
	ofstream m_out_Stream;
	m_out_Stream.open(objPath, fstream::out | fstream::binary);
	m_out_Stream << setiosflags(ios::fixed);
	m_out_Stream << setprecision(6) ;

	//groupname很多地方用
	char* p = m_fileHeader.group_names;
	vector<string> v_names;
	v_names.push_back(p);
	for(int i=0;i<m_fileHeader.group_names_size;i++)
	{ 
		if(*p == 0 && *(p+1) != 0) 
			v_names.push_back(p+1);
		p++;
	}

	m_out_Stream << "(lt-model-0" << endl;
	{
		//on-load-cmds
		m_out_Stream << "\t" << "(on-load-cmds" << endl;  // "\t"好水，先这样写吧
		{
			//set-node-flags option
			m_out_Stream << "\t\t" << "(set-node-flags \r\n" << "\t\t\t(" << endl;
			for(int i=0; i < p_aux_data->nr_aux_elems; i++)
			{
				m_out_Stream << "\t\t\t\t" << "(\"" << bone_names[i] << "\" 0)" << endl;
			}
			m_out_Stream << "\t\t\t) \r\n" <<"\t\t) " <<endl;

			//lod-groups == option 可选，但是不设置会不显示模型
			m_out_Stream << "\t\t" << "(lod-groups \r\n" << "\t\t\t(" << endl;
			for(int i=0; i < m_fileHeader.nr_groups; i++)
			{
				m_out_Stream << "\t\t\t\t" << "(create-lod-group  \"" << v_names[i] << "\"" << endl;
				m_out_Stream << "\t\t\t\t\t" << "(lod-dists(0.000000))" <<endl;
				m_out_Stream << "\t\t\t\t\t" << "(shapes (\"" << v_names[i] << "\"))" << endl;
				m_out_Stream << "\t\t\t\t)"<< endl;
			}
			m_out_Stream << "\t\t\t) \r\n" <<"\t\t) " <<endl;
			
			//deformer
			m_out_Stream << "\t\t" << "(add-deformer" << endl;
			for(int i=0; i < m_fileHeader.nr_groups; i++)
			{
				group_data* p_group_data = &m_group_data[i];  //one group 
				group_header* p_group_header = &p_group_data->group_header;
				int meshCount = p_group_header->flag_b3 & 0xFF;

				v_bones* m_v_bones = new v_bones[p_group_header->verteces_nr];

				for(int j=0; j < meshCount; j++)
				{
					mesh_data* p_mesh_data = &p_group_data->mesh_list[j];  //one mesh data
					mesh_header* p_mesh_header = &p_mesh_data->mesh_header;
					char* z_data = p_mesh_data->z_info.data;

					vector<string> mesh_bone_names;
					char* p = p_mesh_data->names;
					mesh_bone_names.push_back(p);
					for(int k=0;k<p_mesh_data->names_size;k++)
					{ 
						if(*p == 0 && *(p+1) != 0) 
							mesh_bone_names.push_back(p+1);
						p++;
					}

					for(int k=0; k< p_mesh_header->vertex_info_nr ;k++)
					{
						int meshBoneIndex = z_data[k*3];
						string meshBoneStr = mesh_bone_names.at(meshBoneIndex);
						short v_index = p_mesh_data->vertex_info[k].vertex_index;
						int z = 0;
						for(z=0; z < bone_names.size(); z++ ){
							if(strcmp(bone_names.at(z).c_str(),meshBoneStr.c_str())==0)
								break;
						}
						m_v_bones[v_index].bone_indexs.insert(z);
					}
				}
				
				m_out_Stream << "\t\t\t" << "(skel-deformer" << endl;
				m_out_Stream << "\t\t\t\t" << "(target \"" << v_names[i] << "\" )" << endl;
				m_out_Stream << "\t\t\t\t" << "(influences  (";
				for(int z=0; z < bone_names.size(); z++ )
				{
					m_out_Stream << "\"" << bone_names.at(z).c_str() << "\" " ;
				}
				m_out_Stream << "))" <<endl;
				m_out_Stream << "\t\t\t\t" << "(weightsets (" <<endl;
				for(int t=0;t<p_group_header->verteces_nr;t++)
				{
					int c = m_v_bones[t].bone_indexs.size();
					float weight = 1.0f/c;  //todo ==== 这样平均貌似不对
					set<int>::iterator it;
					m_out_Stream << "\t\t\t\t\t (" ;
					for(it=m_v_bones[t].bone_indexs.begin ();it!=m_v_bones[t].bone_indexs.end ();it++)
					{
						m_out_Stream << dec << *it << " " << weight << " ";
					}
					m_out_Stream << ")" <<endl;
				}
				m_out_Stream << "\t\t\t\t" << "))" <<endl;  //weightsets
				m_out_Stream << "\t\t\t" << ")" <<endl;  //skel-deformer
			}
			m_out_Stream << "\t\t"  << ")" <<endl; //add-deformer
		}
		m_out_Stream << "\t" << ")" << endl;

		//hierarchy == must 必须
		m_out_Stream << "\t" << "(hierarchy (children (" <<endl;
		inOrderTraveral( m_out_Stream, p_aux_data->bone_links, &p_aux_data->bone_links[0], 2);
		m_out_Stream << "\t" << "))" <<endl;

		//shapes
		for(int i=0; i < m_fileHeader.nr_groups; i++)
		{
			m_out_Stream << "\t" << "(shape \"" << v_names[i] << "\"" << endl;
			m_out_Stream << "\t\t" << "(geometry" << endl;
			m_out_Stream << "\t\t\t" << "(mesh \"" << v_names[i] << "\"" << endl;
			group_data* p_group_data = &m_group_data[i];  //one group 
			group_header* p_group_header = &p_group_data->group_header;

			m_out_Stream << "\t\t\t\t" << "(vertex(" << endl;
			for(int j=0; j < p_group_header->verteces_nr; j++)
			{
				vertex_coordinate* p_vertece = &p_group_data->verteces[j];
				m_out_Stream << "\t\t\t\t\t(" << p_vertece->x << " " << p_vertece->y << " " << p_vertece->z << ")"<< endl;
			}
			m_out_Stream << "\t\t\t\t" << "))" << endl; //vertex
			m_out_Stream << "\t\t\t\t" << "(normals(" << endl;
			for(int j=0; j < p_group_header->normals_nr; j++)
			{
				normal_vector* p_normal = &p_group_data->normals[j];
				m_out_Stream << "\t\t\t\t\t("  << p_normal->x << " " << p_normal->y << " " << p_normal->z << ")"<< endl;
			}
			m_out_Stream << "\t\t\t\t" << "))" << endl; //normals
			m_out_Stream << "\t\t\t\t" << "(uvs(" << endl;
			for(int j=0; j < p_group_header->uv_nr; j++)
			{
				uv_coordinate* p_uv = &p_group_data->uvs[j];
				m_out_Stream << "\t\t\t\t\t(" << p_uv->u << " " << p_uv->v << ")"<< endl;
			}
			m_out_Stream << "\t\t\t\t" << "))" << endl; //uvs

			m_out_Stream << "\t\t\t\t" << "(tri-fs" << endl;
			m_out_Stream << "\t\t\t\t\t(";
			int tcnt1 = 0;
			int meshCount = p_group_header->flag_b3 & 0xFF;
			for(int j=0; j < meshCount; j++)
			{
				mesh_data* p_mesh_data = &p_group_data->mesh_list[j];  //one mesh data
				mesh_header* p_mesh_header = &p_mesh_data->mesh_header;
				for(int k=0; k < p_mesh_header->face_info_nr; k++) //索引信息
				{
					face_info* p_face_info = &p_mesh_data->face_info[k];
					for(int z = 0;z<3; z++) 
					{
						short vIndex = p_face_info->vi[z]; 
						short vertex_index = p_mesh_data->vertex_info[vIndex].vertex_index ;   
						short uv_index = p_mesh_data->vertex_info[vIndex].uv_index ;
						short normal_index = p_mesh_data->vertex_info[vIndex].normal_index  ; 
						m_out_Stream << vertex_index << " " ;
						tcnt1++;
					}
				}
			}
			m_out_Stream << ")" << endl;
			m_out_Stream << "\t\t\t\t" << ")" << endl; //tri-fs */

			m_out_Stream << "\t\t\t\t" << "(tex-fs" << endl;
			m_out_Stream << "\t\t\t\t\t(";

			for(int j=0; j < meshCount; j++)
			{
				mesh_data* p_mesh_data = &p_group_data->mesh_list[j];  //one mesh data
				mesh_header* p_mesh_header = &p_mesh_data->mesh_header;
				for(int k=0; k < p_mesh_header->face_info_nr; k++) //索引信息
				{
					face_info* p_face_info = &p_mesh_data->face_info[k];
					for(int z = 0;z<3; z++) 
					{
						short vIndex = p_face_info->vi[z]; 
						short vertex_index = p_mesh_data->vertex_info[vIndex].vertex_index ;   
						short uv_index = p_mesh_data->vertex_info[vIndex].uv_index ;
						short normal_index = p_mesh_data->vertex_info[vIndex].normal_index  ; 
						m_out_Stream << uv_index << " " ;
						tcnt1++;
					}
				}
			}
			m_out_Stream << ")" << endl;
			m_out_Stream << "\t\t\t\t" << ")" << endl; //tex-fs

			m_out_Stream << "\t\t\t" << ")" << endl; //mesh
			m_out_Stream << "\t\t" << ")" << endl; //geometry
			m_out_Stream << "\t\t" << "(texture-indices(0))"  << endl;
			m_out_Stream << "\t\t" << "(renderstyle-index 0)"<< endl;
			m_out_Stream << "\t\t" << "(render-priority 0)" <<endl;
			m_out_Stream << "\t" << ")" << endl; //shape
		}
	}
	m_out_Stream << ")" << endl << endl;
}

//========================================write obj=======================
void saveAsOBJ(file_header& m_fileHeader,group_data* m_group_data)
{
	const char* objPath = "..\\out.obj";
	ofstream m_out_Stream;
	m_out_Stream.open(objPath, fstream::out | fstream::binary);
	m_out_Stream << setiosflags(ios::fixed);
	m_out_Stream << setprecision(6) ;
	m_out_Stream << "#groups count:" << dec << m_fileHeader.nr_groups <<endl;
	m_out_Stream << "g default" <<endl;
	//for(int i=0; i < m_fileHeader.nr_groups; i++)
	int i=0; //只输出一个看看效果
	{ 
		group_data* p_group_data = &m_group_data[i];  //one group 
		group_header* p_group_header = &p_group_data->group_header;
		m_out_Stream << "#group" << dec << i <<endl;
		//todo group_names 空格隔开的，需要拆分了写入
		m_out_Stream << "#Geometric vertices " << dec << p_group_header->verteces_nr <<endl;
		for(int j=0; j < p_group_header->verteces_nr; j++)
		{
			vertex_coordinate* p_vertece = &p_group_data->verteces[j];
			m_out_Stream << "v " << p_vertece->x << " " << p_vertece->y << " " << p_vertece->z <<endl;
		}
		m_out_Stream << endl;
		m_out_Stream << "#Vertex normals " << dec  << p_group_header->normals_nr <<endl; 
		for(int j=0; j < p_group_header->normals_nr; j++)
		{
			normal_vector* p_normal = &p_group_data->normals[j];
			m_out_Stream << "vn " << p_normal->x << " " << p_normal->y << " " << p_normal->z <<endl;
		}
		m_out_Stream << endl;
		m_out_Stream << "#Texture vertices " << dec  << p_group_header->uv_nr <<endl;
		for(int j=0; j < p_group_header->uv_nr; j++)
		{
			uv_coordinate* p_uv = &p_group_data->uvs[j];
			m_out_Stream << "vt " << p_uv->u << " " << p_uv->v <<endl;
		}
		m_out_Stream << "s off" <<endl;  //关闭光滑组
		m_out_Stream << endl;
		int mesh_data_size = p_group_header->flag_b3 & 0xFF;
		m_out_Stream << "#mesh count:" << dec  << mesh_data_size <<endl;
		for(int j=0; j < mesh_data_size; j++)
		{
			mesh_data* p_mesh_data = &p_group_data->mesh_list[j];  //one mesh data
			mesh_header* p_mesh_header = &p_mesh_data->mesh_header;
			m_out_Stream << "#mesh  " << dec  << j <<endl;
			//todo names 空格隔开的，需要拆分了写入
			m_out_Stream << "#Faces  " << dec  << p_mesh_header->face_info_nr <<endl;
			m_out_Stream << "g pCube" << dec << j <<endl; //表示组，指把"g pCube1"后出现的面都结合到一起，组成一个整的多边形几何体。
			m_out_Stream << "usemtl initialShadingGroup" <<endl;  //表示使用的材质。
			for(int k=0; k < p_mesh_header->face_info_nr; k++) //索引信息
			{
				face_info* p_face_info = &p_mesh_data->face_info[k];
				m_out_Stream << "f ";  //"f 顶点索引/uv点索引/法线索引 顶点索引/uv点索引/法线索引  ..."
				for(int z = 0;z<3; z++) 
				{
					short vIndex = p_face_info->vi[z]; 
					short vertex_index = p_mesh_data->vertex_info[vIndex].vertex_index + 1;  //obj的索引是1开始的
					short uv_index = p_mesh_data->vertex_info[vIndex].uv_index + 1;
					short normal_index = p_mesh_data->vertex_info[vIndex].normal_index + 1;
					if (p_group_header->normals_nr > 0 && p_group_header->uv_nr > 0) {
						m_out_Stream << vertex_index << "/" << uv_index << "/" << normal_index << " ";  
					} else {
						m_out_Stream << vertex_index << " ";  
					}
				}
				m_out_Stream << endl;
			}
		} //face	
	} 
	m_out_Stream.close();
	//========================================================================
}

int parseDxg(const char* dxgPath)
{
	//bone names
	vector<string> bone_names; //p_aux_data->nr_aux_elems个

	ifstream m_Stream;
	m_Stream.open((char*)dxgPath,ios::in | ios:: binary);

	file_header m_fileHeader;
	m_Stream.read(m_fileHeader.signature, 4); // "DXG"
	m_Stream.read((char*)&m_fileHeader.version, 4);  //1 
	m_Stream.read((char*)&m_fileHeader.flag, 4);  //3 , 1,  7
	m_Stream.read((char*)&m_fileHeader.nr_groups, 4);
	m_Stream.read((char*)&m_fileHeader.next_data_offset, 4);  //aux的开始 groups结束完 350
	m_Stream.read((char*)&m_fileHeader.group_names_size, 4);
	memset(&m_fileHeader.group_names,0, 256);
	m_Stream.read((char*)&m_fileHeader.group_names, m_fileHeader.group_names_size);   //0x24

	//GROUPS
	int testNext = 0;
	group_data* m_group_data = (group_data*)malloc(m_fileHeader.nr_groups * sizeof(group_data));
	for(int i=0; i < m_fileHeader.nr_groups; i++)
	{
		if(i>0)
			m_Stream.seekg(testNext); //其实不需要定位了，已经刚好解析到这里
		group_data* p_group_data = &m_group_data[i];  //one group 
		group_header* p_group_header = &p_group_data->group_header;
		//==one group header==
		m_Stream.read((char*)&p_group_header->flag_b1, 4);  //  03 / 01
		m_Stream.read((char*)&p_group_header->offset_f, 4);	 //从当前位置偏移offset_f就是 aux_data 开始位置 350(注意不是group起始位置，差了8)
		testNext = p_group_header->offset_f + (int)m_Stream.tellg();
		m_Stream.read((char*)&p_group_header->flag_b2, 4); 
		m_Stream.read((char*)&p_group_header->flag_b3, 4);	// 低2个字节是网格数据数，看两个字节是0
		m_Stream.read((char*)&p_group_header->offset_g, 4);  //p_group_header 偏移 offset_g 可以到 group_data.dvs 的结束位置

		m_Stream.read((char*)&p_group_header->verteces_nr, 2); //顶点数 
		m_Stream.read((char*)&p_group_header->normals_nr, 2); //法线数
		m_Stream.read((char*)&p_group_header->uv_nr, 4); //uv贴图/纹理 坐标数 
		m_Stream.read((char*)&p_group_header->weight_map_nr, 4); //权重 存w0,w1, => w0+w1+w2=1.0 的关系

		//mesh datas
		int mesh_count = p_group_header->flag_b3 & 0xFF;
		p_group_data->mesh_list = (mesh_data*)malloc(mesh_count * sizeof(mesh_data));
		for(int j=0; j < mesh_count; j++)
		{
			cout<<"mesh_data start pos: "<<  hex << m_Stream.tellg() << endl;

			mesh_data* p_mesh_data = &p_group_data->mesh_list[j];  //one mesh data
			//==one mesh data header==
			mesh_header* p_mesh_header = &p_mesh_data->mesh_header;
			m_Stream.read((char*)&p_mesh_header->vertex_info_nr, 2); 
			m_Stream.read((char*)&p_mesh_header->face_info_nr, 2); 
			m_Stream.read((char*)&p_mesh_header->name_t_nr, 4); 
			m_Stream.read((char*)&p_mesh_header->data_z_nr, 4); 
			m_Stream.read((char*)&p_mesh_header->verteces_offset, 4);  //当前位置偏移offset是下一个对象(verc或者meshdata)的位置

			cout<<"vertex_info pos: "<<  hex << m_Stream.tellg() << endl;
			cout<<"vertex_info count: "<< dec << p_mesh_header->vertex_info_nr << endl;
			p_mesh_data->vertex_info = (vertex_info*)malloc(sizeof(vertex_info)*p_mesh_header->vertex_info_nr);
			for(int k=0; k < p_mesh_header->vertex_info_nr; k++)  //索引信息
			{
				//==one vertex_info==
				vertex_info* p_vertex_info = &p_mesh_data->vertex_info[k]; 
				m_Stream.read((char*)&p_vertex_info->vertex_index, 2); 
				m_Stream.read((char*)&p_vertex_info->normal_index, 2); 
				m_Stream.read((char*)&p_vertex_info->uv_index, 2); 
				m_Stream.read((char*)&p_vertex_info->unknown1, 2);  //FF
			}

			cout<<"face_info pos: "<<  hex << m_Stream.tellg() << endl;
			cout<<"face_info count: "<< dec << p_mesh_header->face_info_nr << endl;
			p_mesh_data->face_info = (face_info*)malloc(sizeof(face_info)*p_mesh_header->face_info_nr);
			for(int k=0; k < p_mesh_header->face_info_nr; k++) //索引信息
			{
				face_info* p_face_info = &p_mesh_data->face_info[k];
				m_Stream.read((char*)&p_face_info->vi[0], 2); 
				m_Stream.read((char*)&p_face_info->vi[1], 2); 
				m_Stream.read((char*)&p_face_info->vi[2], 2); 
			}
			cout<<"name_t pos: "<<  hex << m_Stream.tellg() << endl;
			cout<<"name_t count: "<< dec << p_mesh_header->name_t_nr << endl;

			if(p_mesh_header->name_t_nr > 0)
			{
				m_Stream.read((char*)&p_mesh_data->names_size, 4); 
				memset(&p_mesh_data->names,0, 256);  //绑定的bones列表
				m_Stream.read((char*)p_mesh_data->names, p_mesh_data->names_size); //用空格隔开的字符串，p_mesh_header->name_t_nr个
			}

			cout<<"z_info pos: "<<  hex << m_Stream.tellg() << endl;
			cout<<"z_info count: "<< dec << p_mesh_header->data_z_nr << endl; //3倍 vertex_info_nr
			if (p_mesh_header->data_z_nr >0)
			{
				p_mesh_data->z_info.data = (char*)malloc(p_mesh_header->data_z_nr); //data_z = vertex_info_nr * 3
				m_Stream.read((char*)p_mesh_data->z_info.data, p_mesh_header->data_z_nr); //里面是 00 - 07的数字
				
			}
		}
		cout<<"verteces pos: "<<  hex << m_Stream.tellg() << endl;
		cout<<"verteces count: "<< dec << p_group_header->verteces_nr << endl;

		//vertex coordinate
		p_group_data->verteces = (vertex_coordinate*)malloc(sizeof(vertex_coordinate)*p_group_header->verteces_nr);
		for(int j=0; j < p_group_header->verteces_nr; j++)
		{
			vertex_coordinate* p_vertece = &p_group_data->verteces[j];
			m_Stream.read((char*)&p_vertece->x, 4); 
			m_Stream.read((char*)&p_vertece->y, 4); 
			m_Stream.read((char*)&p_vertece->z, 4); 
		}
		cout<<"normals pos: "<<  hex << m_Stream.tellg() << endl;
		cout<<"normals count: "<< dec << p_group_header->normals_nr << endl;
		//normal
		p_group_data->normals = (normal_vector*)malloc(sizeof(normal_vector)*p_group_header->normals_nr);
		for(int j=0; j < p_group_header->normals_nr; j++)
		{
			normal_vector* p_normal = &p_group_data->normals[j];
			m_Stream.read((char*)&p_normal->x, 4); 
			m_Stream.read((char*)&p_normal->y, 4); 
			m_Stream.read((char*)&p_normal->z, 4); 
		}

		cout<<"uvs pos: "<<  hex << m_Stream.tellg() << endl;
		cout<<"uvs count: "<< dec << p_group_header->uv_nr << endl;
		//uv
		p_group_data->uvs = (uv_coordinate*)malloc(sizeof(uv_coordinate)*p_group_header->uv_nr);
		for(int j=0; j < p_group_header->uv_nr; j++)
		{
			uv_coordinate* p_uv = &p_group_data->uvs[j];
			m_Stream.read((char*)&p_uv->u, 4); 
			m_Stream.read((char*)&p_uv->v, 4); 
		}
		cout<<"weights pos: "<<  hex << m_Stream.tellg() << endl;
		cout<<"weights count: "<< dec << p_group_header->weight_map_nr << endl;
		//weights
		p_group_data->weight_map = (weight_map*)malloc(sizeof(weight_map)*p_group_header->weight_map_nr/2);
		for(int j=0; j < p_group_header->weight_map_nr/2; j++)
		{
			weight_map* p_weight_map = &p_group_data->weight_map[j];
			m_Stream.read((char*)&p_weight_map->w0, 4);  //
			m_Stream.read((char*)&p_weight_map->w1, 4);  
		}
		cout<<"group end pos: "<<  hex << m_Stream.tellg() << endl;
		cout<<"start parse txt_fs tri_fs " << endl;
		//===解析 tex_fs and tri_fs
		if(m_fileHeader.flag == 3 || m_fileHeader.flag == 7)  //1版本木有这个的，应该是只有模型和uv等，不存在骨骼动画的类型
		{
			//tex_fs
			m_Stream.read((char*)&p_group_data->tex_fs.flag1, 4);
			m_Stream.read((char*)&p_group_data->tex_fs.count, 2);
			m_Stream.read((char*)&p_group_data->tex_fs.flag2, 2);
			m_Stream.read((char*)&p_group_data->tex_fs.alldataSize, 4);
			m_Stream.read((char*)&p_group_data->tex_fs.flag3, 4);
			m_Stream.read((char*)&p_group_data->tex_fs.flag4, 4);
			m_Stream.read((char*)&p_group_data->tex_fs.flag5, 4);
			p_group_data->tex_fs.dat = (tf_data*)malloc(sizeof(tf_data) * p_group_data->tex_fs.count);
			for(int j=0; j < p_group_data->tex_fs.count; j++) //跟改group中mesh count个数一样
			{
				tf_data* p_dat = &p_group_data->tex_fs.dat[j];			
				m_Stream.read((char*)&p_dat->flag1, 2);
				m_Stream.read((char*)&p_dat->count, 2);
				m_Stream.read((char*)&p_dat->flag2, 4);
				m_Stream.read((char*)&p_dat->flag3, 4);
				m_Stream.read((char*)&p_dat->dataSize, 4);

				p_dat->data = (short*)malloc(sizeof(short)* p_dat->count * 3);
				for(int k=0; k< p_dat->count * 3; k++)
				{
					m_Stream.read((char*)&p_dat->data[k], 2);
				}
			}
			//tri_fs
			m_Stream.read((char*)&p_group_data->tri_fs.flag1, 4);
			m_Stream.read((char*)&p_group_data->tri_fs.count, 2);
			m_Stream.read((char*)&p_group_data->tri_fs.flag2, 2);
			m_Stream.read((char*)&p_group_data->tri_fs.alldataSize, 4);
			m_Stream.read((char*)&p_group_data->tri_fs.flag3, 4);
			m_Stream.read((char*)&p_group_data->tri_fs.flag4, 4);
			m_Stream.read((char*)&p_group_data->tri_fs.flag5, 4);
			p_group_data->tri_fs.dat = (tf_data*)malloc(sizeof(tf_data) * p_group_data->tri_fs.count);
			for(int j=0; j < p_group_data->tri_fs.count; j++)
			{
				tf_data* p_dat = &p_group_data->tri_fs.dat[j];			
				m_Stream.read((char*)&p_dat->flag1, 2);
				m_Stream.read((char*)&p_dat->count, 2);
				m_Stream.read((char*)&p_dat->flag2, 4);
				m_Stream.read((char*)&p_dat->flag3, 4);
				m_Stream.read((char*)&p_dat->dataSize, 4);

				p_dat->data = (short*)malloc(sizeof(short)* p_dat->count * 3);
				for(int k=0; k< p_dat->count * 3; k++)
				{
					m_Stream.read((char*)&p_dat->data[k], 2);
				}
			}
		}
		cout<<"tri_fs end pos: "<<  hex << m_Stream.tellg() << endl;
		cout<<"[next group /Aux] start pos: "<<  hex << testNext << endl;
	}

	aux_data m_aux_list[1];  //aux_data* m_aux_list; 
	aux_data* p_aux_data = &m_aux_list[0];

	if(m_fileHeader.flag == 3 || m_fileHeader.flag == 7)  //1版本也没bone这些
	{
		//===Aux_list
		m_Stream.seekg(testNext); //不需要定位了，已经刚好解析到这里
		cout<<"aux_data start pos: "<<  hex << m_Stream.tellg() << endl;

		m_Stream.read((char*)&p_aux_data->nr_aux_elems, 4);   //2
		m_Stream.read((char*)&p_aux_data->aux_data_size, 4);    //A0
		m_Stream.read((char*)&p_aux_data->aux_names_size, 4);   //14
		p_aux_data->aux_names = (char*)malloc(p_aux_data->aux_names_size);
		m_Stream.read((char*)p_aux_data->aux_names, p_aux_data->aux_names_size); //root crystalBox01

		char* p = p_aux_data->aux_names;
		bone_names.push_back(p);
		for(int i=0;i<p_aux_data->aux_names_size;i++)
		{ 
			if(*p == 0 && *(p+1) != 0) 
				bone_names.push_back(p+1);
			p++;
		}

		p_aux_data->bone_links = (bone_link*)malloc(p_aux_data->nr_aux_elems*sizeof(bone_link));
		for(int i=0 ;i < p_aux_data->nr_aux_elems; i++){ //二叉树
			bone_link* p_bone_link = &p_aux_data->bone_links[i];
			m_Stream.read((char*)&p_bone_link->index, 1);
			m_Stream.read((char*)&p_bone_link->parent, 1); //父
			m_Stream.read((char*)&p_bone_link->child, 1); //子
			m_Stream.read((char*)&p_bone_link->sibling, 1); //兄弟
			p_bone_link->name = new string(bone_names.at(i));
		}

		for(int i=0 ;i < p_aux_data->nr_aux_elems; i++){
			bone_link* p_bone_link = &p_aux_data->bone_links[i];
			for(int j=0; j< 16 ;j++){
				m_Stream.read((char*)&p_bone_link->v[j], 4);
			}
		}
	}
	//==
	m_Stream.close();

	if(m_fileHeader.flag == 3 || m_fileHeader.flag == 7) 
	{
		saveAsLTA(m_fileHeader,m_group_data,p_aux_data,bone_names);
	}

	if(m_fileHeader.flag == 1) 
	{
		saveAsOBJ(m_fileHeader,m_group_data);
	}
	//======

	cout << "Parse Dxg File Success!" << endl;

	return 0;
}

void saveAnimAsLTA(const struct MRBAnimationData &data, const char *path, char* animname)
{
	ofstream m_out_Stream;
	m_out_Stream.open(path, fstream::out | fstream::binary);
	m_out_Stream << setiosflags(ios::fixed);
	m_out_Stream << setprecision(6) ;

	uint32_t record_count;
	uint32_t frame_count;
	uint32_t bone_count;
	bone_count = (uint32_t)(data.data.size());
	frame_count = (uint32_t)(data.data.at(0).frames.size());

	m_out_Stream << "(animset \"" << animname <<"\"" <<endl; 
	{
		//keyframe
		m_out_Stream <<"\t" << "(keyframe (keyframe  " << endl;
		{
			m_out_Stream <<"\t\t" << "(times ("; 
			for(int i=0;i<data.keys.size();i++)
			{
				m_out_Stream << dec << data.keys.at(i) << " "; 
			}
			m_out_Stream <<"))" << endl; 
			m_out_Stream <<"\t\t" << "(values ("; 
			for(int i=0;i<data.keys.size();i++)
			{
				m_out_Stream << "\"\"" << " "; 
			}
			m_out_Stream <<"))" << endl; 
		}
		m_out_Stream <<"\t))" <<endl;   //==keyframe

		//anims 
		m_out_Stream << "\t" << "(anims (" <<endl;
		{
			for (uint32_t bone = 0; bone < bone_count; bone++) {  //one bone one anim
				const struct MRBAnimationBone &b = data.data.at(bone);
				m_out_Stream << "\t\t" <<"(anim (parent \"" << b.name << "\")" <<endl;
				m_out_Stream << "\t\t\t" << "(frames (posquat (" << endl; //frames
				for (uint32_t frame = 0; frame < frame_count; frame++) {
					const struct MRBAnimationBoneData &d = b.frames.at(frame);
					m_out_Stream << "\t\t\t\t(";
					m_out_Stream << "(" << d.move.x << " " << d.move.y << " " << d.move.z << ")";
					m_out_Stream << "(" << d.rotation.x << " " << d.rotation.y << " " << d.rotation.z << " " << d.rotation.w << ")";
					m_out_Stream << ")" << endl;
				}
				m_out_Stream << "\t\t\t)))" << endl;
				m_out_Stream << "\t\t)" << endl; //one bone
			}
		}
		m_out_Stream << "\t" << "))" <<endl;  //==anims
	}
	m_out_Stream <<")" <<endl; //==animset
	m_out_Stream.close();
}

int parseMRB(const char* mrbPath)
{
	const char* animPath = "..\\anim.lta";

	struct MRBFile mrb_file;
	int s = read_mrb_file(mrbPath, &mrb_file);
	if (s != 0) {
		fprintf(stderr, "Could not read file. %s\n", mrbPath);
		return s;
	}

	for (int i=0; i < mrb_file.data.size(); i++)
	{
		MRBData* mrb_data = &mrb_file.data[i];
		if (mrb_data->header.type == MRB_TYPE_ANIMATION) {
			saveAnimAsLTA(mrb_data->anim, animPath, mrb_data->header.name);
		}
	}

	cout << "Parse MRB File Success!" << endl;
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	/*const char* dxgPath = "E:\\FPSFExtractor\\testdata\\decrypt\\Chara\\4\\00\\GM4_00_000.dxg";
	parseDxg(dxgPath);*/

	const char* mrbPath = "E:\\FPSFExtractor\\testdata\\decrypt\\Chara\\0\\P00\\PM00.MRB";
	parseMRB(mrbPath);

	system("PAUSE");
	return 0;
}