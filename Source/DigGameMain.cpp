// Render Example.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "Render/Render.h"
#include "Surf/HighResolutionClock.h"
#include "Surf/KeyCodes.h"
#include "Surf/SurfMath.h"


#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/examples/imgui_impl_win32.h"
#include "ImGui/imgui_impl_render.h"

struct
{
	u32 w = 0;
	u32 h = 0;
	float nearZ = 0.1f;
	float farZ = 10'000.0f;
	float fov = 45.0f;
	float aspectRatio = 0.0f;
	matrix projection;
	Texture_t DepthTex = Texture_t::INVALID;
} screenData;

struct
{
	float3 position;
	float3 lookDir;
	float camPitch = 0.0f;
	float camYaw = 0.0f;
	matrix view;
} viewData;

enum class MeshBuffer : u8
{
	POSITION,
	NORMAL,
	COUNT,
};

struct Mesh
{
	VertexBuffer_t vertexBufs[(u8)MeshBuffer::COUNT] = {VertexBuffer_t::INVALID};
	IndexBuffer_t indexBuf = IndexBuffer_t::INVALID;
	RenderFormat indexType = RenderFormat::UNKNOWN;
	u32 indexCount = 0;
	u32 strides[(u8)MeshBuffer::COUNT] = {0};
	u32 offsets[(u8)MeshBuffer::COUNT] = {0};
};

struct MeshMaterial
{
	GraphicsPipelineState_t pso;
};

static void ResizeTargets(u32 w, u32 h)
{
	w = Max(w, 1u);
	h = Max(h, 1u);

	if (w == screenData.w && h == screenData.h)
		return;

	screenData.w = w;
	screenData.h = h;

	screenData.aspectRatio = (float)w / (float)h;

	screenData.projection = MakeMatrixPerspectiveFovLH(ConvertToRadians(screenData.fov), screenData.aspectRatio, screenData.nearZ, screenData.farZ);

	Render_Release(screenData.DepthTex);

	TextureCreateDesc desc = {};
	desc.width = w;
	desc.height = h;
	desc.format = RenderFormat::D32_FLOAT;
	desc.flags = RenderResourceFlags::DSV;
	screenData.DepthTex = CreateTexture(desc);
}

static void UpdateView(const float3& position, float pitch, float yaw)
{
	viewData.position = position;

	if (yaw > 360.0f)
		yaw -= 360.0f;

	if (yaw < -360.0f)
		yaw += 360.0f;

	viewData.camPitch = pitch;
	viewData.camYaw = yaw;

	pitch = Clamp(pitch, -89.9f, 89.9f);

	yaw = ConvertToRadians(yaw);
	pitch = ConvertToRadians(pitch);

	float cosPitch = cosf(pitch);

	viewData.lookDir = float3{cosf(yaw) * cosPitch, sinf(pitch), sinf(yaw) * cosPitch};

	viewData.view = MakeMatrixLookToLH(position, viewData.lookDir, float3{ 0, 1, 0 });
}

static Mesh CreateCubeMesh(float size)
{
	Mesh mesh;

	std::vector<float3> normals;
	normals.resize(6 * 4);

	{
		const float3 ftl = float3(-size, size, size);
		const float3 ftr = float3(size, size, size);
		const float3 fbr = float3(size, -size, size);
		const float3 fbl = float3(-size, -size, size);

		const float3 btl = float3(-size, size, -size);
		const float3 btr = float3(size, size, -size);
		const float3 bbr = float3(size, -size, -size);
		const float3 bbl = float3(-size, -size, -size);

		std::vector<float3> positions;
		positions.resize(6 * 4);

		auto it = positions.begin();

		*it++ = { ftl }; *it++ = { ftr }; *it++ = { fbr }; *it++ = { fbl };
		*it++ = { btr }; *it++ = { btl }; *it++ = { bbl }; *it++ = { bbr };
		*it++ = { ftr }; *it++ = { btr }; *it++ = { bbr }; *it++ = { fbr };
		*it++ = { btl }; *it++ = { ftl }; *it++ = { fbl }; *it++ = { bbl };
		*it++ = { fbl }; *it++ = { fbr }; *it++ = { bbr }; *it++ = { bbl };
		*it++ = { ftl }; *it++ = { btl }; *it++ = { btr }; *it++ = { ftr };

		mesh.vertexBufs[(u8)MeshBuffer::POSITION] = CreateVertexBuffer(positions.data(), positions.size() * sizeof(float3));
		mesh.strides[(u8)MeshBuffer::POSITION] = (u32)sizeof(float3);
		mesh.offsets[(u8)MeshBuffer::POSITION] = 0u;
	}

	{
		constexpr float3 front{ 0, 0, 1 };
		constexpr float3 back{ 0, 0, -1 };
		constexpr float3 left{ 1, 0, 0 };
		constexpr float3 right{ -1, 0, 0 };
		constexpr float3 bottom{ 0, -1, 0 };
		constexpr float3 top{ 0, 1, 0 };

		std::vector<float3> normals;
		normals.resize(6 * 4);

		auto it = normals.begin();

		*it++ = front; *it++ = front; *it++ = front; *it++ = front;
		*it++ = back; *it++ = back; *it++ = back; *it++ = back;
		*it++ = left; *it++ = left; *it++ = left; *it++ = left;
		*it++ = right; *it++ = right; *it++ = right; *it++ = right;
		*it++ = bottom; *it++ = bottom; *it++ = bottom; *it++ = bottom;
		*it++ = top; *it++ = top; *it++ = top; *it++ = top;

		mesh.vertexBufs[(u8)MeshBuffer::NORMAL] = CreateVertexBuffer(normals.data(), normals.size() * sizeof(float3));
		mesh.strides[(u8)MeshBuffer::NORMAL] = (u32)sizeof(float3);
		mesh.offsets[(u8)MeshBuffer::NORMAL] = 0u;
	}

	std::vector<u16> indices;
	indices.resize(6 * 6);

	{
		auto it = indices.begin();
		for (u32 i = 0; i < 6 * 4; i += 4)
		{
			*it++ = i + 2; *it++ = i + 1; *it++ = i;
			*it++ = i; *it++ = i + 3; *it++ = i + 2;
		}
	}

	mesh.indexCount = indices.size();
	mesh.indexBuf = CreateIndexBuffer(indices.data(), indices.size() * sizeof(u16));
	mesh.indexType = RenderFormat::R16_UINT;

	return mesh;
}

static MeshMaterial CreateMaterial()
{
	GraphicsPipelineStateDesc desc = {};
	desc.RasterizerDesc(PrimitiveTopologyType::Triangle, FillMode::Solid, CullMode::Back);
	desc.DepthDesc(true, ComparisionFunc::LessEqual);
	desc.numRenderTargets = 1;
	desc.blendMode[0].None();

	const char* shaderPath = "../Content/Shaders/Mesh.hlsl";

	desc.vs = CreateVertexShader(shaderPath);
	desc.ps = CreatePixelShader(shaderPath);

	InputElementDesc inputDesc[] =
	{
		{"POSITION", 0, RenderFormat::R32G32B32_FLOAT, 0, 0, InputClassification::PerVertex, 0 },
		{"NORMAL", 0, RenderFormat::R32G32B32_FLOAT, 1, 0, InputClassification::PerVertex, 0 },
	};

	MeshMaterial mat;
	mat.pso = CreateGraphicsPipelineState(desc, inputDesc, ARRAYSIZE(inputDesc));

	return mat;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int main()
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"Render Example", NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, L"Render Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

	if (!Render_Init())
	{
		Render_ShutDown();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	{
		std::vector<SamplerDesc> samplers(1);
		samplers[0].AddressModeUVW(SamplerAddressMode::Wrap).FilterModeMinMagMip(SamplerFilterMode::Point);

		InitSamplers(samplers.data(), samplers.size());
	}	

	RenderViewPtr view = CreateRenderViewPtr((intptr_t)hwnd);

	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplRender_Init();

	HighResolutionClock updateClock;

	UpdateView(float3{ -2, 6, -2 }, 0.0f, 45.0f);

	// Set up entities
	MeshMaterial material = CreateMaterial();
	Mesh cubeMesh = CreateCubeMesh(0.5f);

	// Set up grid
	constexpr u32 gridDim = 32;
	constexpr u32 gridSize = gridDim * gridDim;

	std::vector<float3> meshPositions;
	meshPositions.resize(gridSize);

	{
		auto it = meshPositions.begin();

		for (u32 y = 0; y < gridDim; y++)
		{
			for (u32 x = 0; x < gridDim; x++)
			{
				*it++ = float3(x, 0, y);
			}
		}
	}

	// Main loop
	bool bQuit = false;
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (bQuit == false && msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}

		updateClock.Tick();

		float delta = (float)updateClock.GetDeltaSeconds();

		// ImGui stuff
		ImGui_ImplRender_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();
		ImGui::ShowDemoWindow();
		ImGui::Render();

		ImGuiIO& io = ImGui::GetIO();

		float camPitch = viewData.camPitch;
		float camYaw = viewData.camYaw;

		if (!io.WantCaptureMouse && io.MouseDown[1])
		{
			float yaw = ImGui::GetIO().MouseDelta.x;
			float pitch = ImGui::GetIO().MouseDelta.y;

			camPitch -= pitch * 25.0f * delta;
			camYaw -= yaw * 25.0f * delta;
		}

		float3 translation = { 0.0f };
		
		if (!io.WantCaptureKeyboard)
		{	
			float3 fwd = viewData.lookDir;
			float3 rgt = CrossF3(float3{0, 1, 0}, viewData.lookDir);

			constexpr float speed = 1.0f;

			float moveSpeed = speed * delta;

			float3 translateDir = 0.0f;

			if (io.KeysDown[KeyCode::W]) translateDir += fwd * moveSpeed;
			if (io.KeysDown[KeyCode::S]) translateDir -= fwd * moveSpeed;

			if (io.KeysDown[KeyCode::D]) translateDir += rgt * moveSpeed;
			if (io.KeysDown[KeyCode::A]) translateDir -= rgt * moveSpeed;

			translation = NormalizeF3(translateDir);

			if (io.KeysDown[KeyCode::E]) translation.y += moveSpeed;
			if (io.KeysDown[KeyCode::Q]) translation.y -= moveSpeed;
		}		

		UpdateView(viewData.position + translation, camPitch, camYaw);

		// Begin render frame
		Render_NewFrame();
		CommandListPtr cl = CommandList::Create();

		view->ClearCurrentBackBufferTarget(cl.get());

		DepthStencilView_t dsv = GetTextureDSV(screenData.DepthTex);

		if (dsv != DepthStencilView_t::INVALID)
			cl->ClearDepth(dsv, 1.0f);

		RenderTargetView_t backBufferRtv = view->GetCurrentBackBufferRTV();
		cl->SetRenderTargets(&backBufferRtv, 1, dsv);

		// Set up view 
		Viewport vp;
		vp.width = static_cast<float>(screenData.w);
		vp.height = static_cast<float>(screenData.h);
		vp.minDepth = 0;
		vp.maxDepth = 1;
		vp.topLeftX = 0;
		vp.topLeftY = 0;

		cl->SetViewports(&vp, 1);
		cl->SetDefaultScissor();

		struct
		{
			matrix viewProjMat;
			float3 camPos;
			float pad;
		} viewBufData;

		viewBufData.viewProjMat = viewData.view * screenData.projection;
		viewBufData.camPos = viewData.position;

		DynamicBuffer_t viewBuf = CreateDynamicConstantBuffer(&viewBufData, sizeof(viewBufData));

		cl->BindVertexCBVs(0, 1, &viewBuf);
		cl->BindPixelCBVs(0, 1, &viewBuf);

		// Prepare to draw mesh
		cl->SetPipelineState(material.pso);

		cl->SetVertexBuffers(0, (u32)MeshBuffer::COUNT, cubeMesh.vertexBufs, cubeMesh.strides, cubeMesh.offsets);
		cl->SetIndexBuffer(cubeMesh.indexBuf, cubeMesh.indexType, 0);

		for (const float3& position : meshPositions)
		{
			struct
			{
				matrix transform;
			} transformData;

			transformData.transform = MakeMatrixTranslation(position);
			DynamicBuffer_t transformBuf = CreateDynamicConstantBuffer(&transformData, sizeof(transformData));

			cl->BindVertexCBVs(1, 1, &transformBuf);

			cl->DrawIndexedInstanced(cubeMesh.indexCount, 1, 0, 0, 0);
		}

		ImGui_ImplRender_RenderDrawData(ImGui::GetDrawData(), cl.get());

		CommandList::Execute(cl);
		view->Present(true);
	}

	ImGui_ImplRender_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	Render_ShutDown();

	::DestroyWindow(hwnd);
	::UnregisterClass(wc.lpszClassName, wc.hInstance);
}

// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	RenderView* rv = GetRenderViewForHwnd((intptr_t)hWnd);

	switch (msg)
	{
	case WM_MOVE:
	{
		RECT r;
		GetWindowRect(hWnd, &r);
		const int x = (int)(r.left);
		const int y = (int)(r.top);
		break;
	}
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			const int w = (int)LOWORD(lParam);
			const int h = (int)HIWORD(lParam);

			if (rv)	rv->Resize(w, h);

			ResizeTargets(w, h);
			return 0;
		}

	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
