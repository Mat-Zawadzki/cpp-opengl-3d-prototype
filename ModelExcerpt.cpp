/*
  Selected code excerpt from my private C++ OpenGL RTS game engine.

  This file is not intended to compile standalone.
  It is included to demonstrate implementation style and technical approach
  without publishing the full engine source code.
*/

#include "Model.h"

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexBufferLayout.h"
#include "Texture.h"
#include "Colour.h"

/*
  Notes:
  - This is a trimmed excerpt, not the full Model.cpp file.
  - The enum-to-asset path table and simple quad helpers are intentionally omitted.
  - Project-specific types such as EntityEnum, TextureManager, InstanceData,
    Triangle and ColourEnum are defined in the private source tree.
*/

void Model::MakeModel(EntityEnum which_model, TextureManager* TXTR_MNGR)
{
    if (!loaded_onto_RAM)
    {
        LoadModelOntoRAM(which_model);
    }

    if (!loaded_onto_VRAM)
    {
        LoadModelOntoVRAM(which_model, TXTR_MNGR);
    }
}

void Model::LoadModelOntoRAM(EntityEnum which_model)
{
    if (loaded_onto_RAM)
    {
        return;
    }

    const std::string base_path = "assets\\Models\\" + EnumToString(which_model) + "\\";

    const std::string vertex_filepath = base_path + "Vertices_VBO.txt";
    const std::string triangle_filepath = base_path + "Triangle_IBO.txt";
    const std::string line_filepath = base_path + "Line_IBO.txt";
    const std::string hitbox_vertex_filepath = base_path + "Hitbox_VBO.txt";
    const std::string hitbox_triangle_filepath = base_path + "Hitbox_Triangle_IBO.txt";

    ReadFile(vertex_filepath, m_vertices_VEC);
    ReadFile(triangle_filepath, m_triangle_index_VEC);

    // Only load line indices if you actually use them and the file exists.
    // ReadFile(line_filepath, m_line_index_VEC);

    ReadFile(hitbox_vertex_filepath, hitbox_vertices_VEC);
    ReadFile(hitbox_triangle_filepath, hitbox_triangle_index_VEC);

    CreateTriangleBuffer(hitbox_vertices_VEC, hitbox_triangle_index_VEC);

    loaded_onto_RAM = true;
}

void Model::LoadModelOntoVRAM(EntityEnum which_model, TextureManager* TXTR_MNGR)
{
    if (loaded_onto_VRAM)
    {
        return;
    }

    if (!loaded_onto_RAM)
    {
        std::cerr << "LoadModelOntoVRAM called before RAM load for model: " << EnumToString(which_model) << "\n";
        return;
    }

    const int vbo_space = static_cast<int>(m_vertices_VEC.size() * sizeof(float));

    m_VBO = std::make_unique<VertexBuffer>(m_vertices_VEC.data(), vbo_space);
    m_VAO = std::make_unique<VertexArray>();
    TRI_IBO = std::make_unique<IndexBuffer>(m_triangle_index_VEC.data(), static_cast<unsigned int>(m_triangle_index_VEC.size()));
    LINE_IBO = std::make_unique<IndexBuffer>(m_line_index_VEC.data(), static_cast<unsigned int>(m_line_index_VEC.size()));

    VertexBufferLayout layout;
    layout.Push<float>(3);
    layout.Push<float>(3);
    layout.Push<float>(2);

    m_VAO->AddBuffer(*m_VBO, layout);

    if (which_model == TREE_1 || which_model == GRASS_1_1)
    {
        const int instance_vbo_space = static_cast<int>(sizeof(InstanceData));

        InstanceData defaultInstance{};
        defaultInstance.model = glm::mat4(1.0f);


        std::vector<ColourEnum> forestColours =
        {
            ColourEnum::GREEN_1,
            ColourEnum::GREEN_2,
            ColourEnum::GREEN_3
        };

        defaultInstance.colour_variant = GetRandomColourUV(forestColours);

        m_instances_VBO = std::make_unique<VertexBuffer>(&defaultInstance, instance_vbo_space);

        m_VAO->Bind();

        VertexBufferLayout instanceLayout;
        instanceLayout.Push<float>(4);
        instanceLayout.Push<float>(4);
        instanceLayout.Push<float>(4);
        instanceLayout.Push<float>(4);
        instanceLayout.Push<float>(2);

        m_VAO->AddBuffer(*m_instances_VBO, instanceLayout, true, 3);
    }

    m_Texture = TXTR_MNGR->UseTexture(AutumnTexture);

    const bool keep_cpu_vectors =
        (which_model == FLOOR_1_TERRAIN) ||
        (which_model == SELECTION_SQUARE);

    if (!keep_cpu_vectors)
    {
        ClearVectors();
    }

    loaded_onto_VRAM = true;
}

template <typename T>
void Model::ReadFile(const std::string& filepath, std::vector<T>& passed_Vec)
{
    std::ifstream file(filepath);

    if (!file.is_open())
    {
        std::cerr << "Error opening file: " << filepath << std::endl;
        return;
    }

    std::string firstLine;
    if (!std::getline(file, firstLine))
    {
        std::cerr << "Error reading first line from file: " << filepath << std::endl;
        return;
    }

    int numValues = 0;
    try
    {
        numValues = std::stoi(firstLine);
    }
    catch (const std::exception&)
    {
        std::cerr << "Invalid count header in file: " << filepath << std::endl;
        return;
    }

    passed_Vec.clear();
    passed_Vec.reserve(static_cast<size_t>(numValues));

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        T value;
        while (iss >> value)
        {
            passed_Vec.push_back(value);
            if (iss.peek() == ',')
            {
                iss.ignore();
            }
        }
    }
}

void Model::UpdateInstanceVBO()
{
	if (entity_positions.empty() || !m_VAO)
	{
		return;
	}

	m_VAO->RemoveBuffer(3, 5);

	const int vbo_space = static_cast<int>(entity_positions.size() * sizeof(InstanceData));

	m_instances_VBO = std::make_unique<VertexBuffer>(entity_positions.data(), vbo_space);

	m_VAO->Bind();

	VertexBufferLayout instanceLayout;
	instanceLayout.Push<float>(4);
	instanceLayout.Push<float>(4);
	instanceLayout.Push<float>(4);
	instanceLayout.Push<float>(4);
	instanceLayout.Push<float>(2);

	m_VAO->AddBuffer(*m_instances_VBO, instanceLayout, true, 3);
}

void Model::ClearVectors()
{
    m_vertices_VEC.clear();
    m_triangle_index_VEC.clear();
    m_line_index_VEC.clear();
    hitbox_vertices_VEC.clear();
    hitbox_triangle_index_VEC.clear();
}

void Model::CreateTriangleBuffer(const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
{
    m_Triangle_VEC.clear();
    m_Triangle_VEC.reserve(indices.size() / 3);

    for (size_t i = 0; i < indices.size() / 3; ++i)
    {
        m_Triangle_VEC.push_back(CreateTriangleFromBuffer(static_cast<std::uint32_t>(i), vertices, indices));
    }
}

Triangle Model::CreateTriangleFromBuffer(std::uint32_t index,
    const std::vector<float>& vertices,
    const std::vector<unsigned int>& indices)
{
    Triangle triangle{};

    const std::size_t i0 = static_cast<std::size_t>(index) * 3;
    if (i0 + 2 >= indices.size())
    {
        return triangle;
    }

    const std::size_t v0 = static_cast<std::size_t>(indices[i0]) * 8;
    const std::size_t v1 = static_cast<std::size_t>(indices[i0 + 1]) * 8;
    const std::size_t v2 = static_cast<std::size_t>(indices[i0 + 2]) * 8;

    if (v0 + 2 >= vertices.size() || v1 + 2 >= vertices.size() || v2 + 2 >= vertices.size())
    {
        return triangle;
    }

    triangle.v0.x = vertices[v0];
    triangle.v0.y = vertices[v0 + 1];
    triangle.v0.z = vertices[v0 + 2];

    triangle.v1.x = vertices[v1];
    triangle.v1.y = vertices[v1 + 1];
    triangle.v1.z = vertices[v1 + 2];

    triangle.v2.x = vertices[v2];
    triangle.v2.y = vertices[v2 + 1];
    triangle.v2.z = vertices[v2 + 2];

    return triangle;
}


// ======================================================
// ModelManager excerpt
// ======================================================

Model* ModelManager::FindModelNoLock(EntityEnum which_model) const
{
    const auto it = m_models.find(which_model);
    return (it != m_models.end()) ? it->second.get() : nullptr;
}

void ModelManager::LoadModelOntoRAM(EntityEnum which_model)
{
    // smaller scope so I destroy + unlock mutex once check complete
    {
        std::lock_guard<std::mutex> lock(m_models_mutex);
        if (FindModelNoLock(which_model) != nullptr)
        {
            return;
        }
    }

    std::unique_ptr<Model> new_model = std::make_unique<Model>();
    new_model->LoadModelOntoRAM(which_model);

    {
        std::lock_guard<std::mutex> lock(m_models_mutex);

        if (FindModelNoLock(which_model) == nullptr)
        {
            m_models.emplace(which_model, std::move(new_model));
        }
    }
}

void ModelManager::LoadModelOntoRAM(EntityEnum which_model)
{
    // smaller scope so I destroy + unlock mutex once check complete
    {
        std::lock_guard<std::mutex> lock(m_models_mutex);
        if (FindModelNoLock(which_model) != nullptr)
        {
            return;
        }
    }

    std::unique_ptr<Model> new_model = std::make_unique<Model>();
    new_model->LoadModelOntoRAM(which_model);

    {
        std::lock_guard<std::mutex> lock(m_models_mutex);

        if (FindModelNoLock(which_model) == nullptr)
        {
            m_models.emplace(which_model, std::move(new_model));
        }
    }
}

Model* ModelManager::UseModel(EntityEnum which_model)
{
    Model* model_ptr = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_models_mutex);
        model_ptr = FindModelNoLock(which_model);
    }

    if (model_ptr == nullptr)
    {
        // Full path: RAM + VRAM. Keep this on main thread only.
        LoadModel(which_model);

        std::lock_guard<std::mutex> lock(m_models_mutex);
        return FindModelNoLock(which_model);
    }

    if (!model_ptr->loaded_onto_RAM)
    {
        std::cerr << "Model exists in manager but is not loaded onto RAM: "
            << model_ptr->EnumToString(which_model) << "\n";
        return model_ptr;
    }

    if (!model_ptr->loaded_onto_VRAM)
    {
        // Main-thread only.
        model_ptr->LoadModelOntoVRAM(which_model, texture_manager);
    }

    return model_ptr;
}


/*
  Omitted from this excerpt:
  - full enum-to-asset path map
  - constructor/destructor boilerplate
  - image quad / framebuffer quad helper models
  - less relevant map query helpers
*/
