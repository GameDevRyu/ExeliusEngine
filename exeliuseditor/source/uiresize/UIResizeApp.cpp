#include "UIResizeApp.h"

#include <include/Input.h>
#include <include/Log.h>

bool UIResizeApp::Initialize()
{
	auto* pGameObjectSystem = Exelius::GameObjectSystem::GetInstance();
	EXE_ASSERT(pGameObjectSystem);

	eastl::string uiImage = "assets/ui/gameobjects/uitesta.json";
	eastl::string uiImageB = "assets/ui/gameobjects/uitestb.json";

	m_activeImage = pGameObjectSystem->CreateGameObject(uiImage, Exelius::CreationMode::kQueueAndSignal);
	m_inactiveImage = pGameObjectSystem->CreateGameObject(uiImageB, Exelius::CreationMode::kQueueAndSignal);

	const auto& pInactiveObject = pGameObjectSystem->GetGameObject(m_inactiveImage);
	const auto& pActiveObject = pGameObjectSystem->GetGameObject(m_activeImage);

	pActiveObject->SetEnabled(true);
	pInactiveObject->SetEnabled(false);

	return true;
}

void UIResizeApp::Update()
{
	if (Exelius::IsKeyReleased(Exelius::KeyCode::Space))
	{
		SwapImagesAndReset();
	}

	MoveImage();
	StretchImage();
}

void UIResizeApp::ShutDown()
{
	auto* pGameObjectSystem = Exelius::GameObjectSystem::GetInstance();
	EXE_ASSERT(pGameObjectSystem);

	pGameObjectSystem->DestroyGameObject(m_activeImage);
	pGameObjectSystem->DestroyGameObject(m_inactiveImage);
}

void UIResizeApp::SwapImagesAndReset()
{
	auto* pGameObjectSystem = Exelius::GameObjectSystem::GetInstance();
	EXE_ASSERT(pGameObjectSystem);

	// Swap the images.
	Exelius::GameObjectID tempID = m_activeImage;
	m_activeImage = m_inactiveImage;
	m_inactiveImage = tempID;

	const auto& pInactiveObject = pGameObjectSystem->GetGameObject(m_inactiveImage);
	pInactiveObject->SetEnabled(false);

	const auto& pActiveObject = pGameObjectSystem->GetGameObject(m_activeImage);
	pActiveObject->SetEnabled(true);

	// Reset the transform of the inactive object.
	if (auto tranform = pInactiveObject->GetComponent<Exelius::TransformComponent>())
	{
		tranform->SetX(0.0f);
		tranform->SetY(0.0f);
	}

	if (auto ui = pInactiveObject->GetComponent<Exelius::UIComponent>())
	{
		ui->Reset();
	}
}

void UIResizeApp::MoveImage()
{
	auto* pGameObjectSystem = Exelius::GameObjectSystem::GetInstance();
	EXE_ASSERT(pGameObjectSystem);

	const auto& pObject = pGameObjectSystem->GetGameObject(m_activeImage);
	if (auto transform = pObject->GetComponent<Exelius::TransformComponent>())
	{
		if (Exelius::IsKeyDown(Exelius::KeyCode::W))
			transform->SetY(transform->GetY() - kPictureSpeed * Exelius::Time::DeltaTime.GetAsSeconds());
		else if (Exelius::IsKeyDown(Exelius::KeyCode::S))
			transform->SetY(transform->GetY() + kPictureSpeed * Exelius::Time::DeltaTime.GetAsSeconds());

		if (transform->GetY() < 0.0f)
			transform->SetY(0.0f);

		if (transform->GetY() > 384.0f)
			transform->SetY(384.0f);

		if (Exelius::IsKeyDown(Exelius::KeyCode::A))
			transform->SetX(transform->GetX() - kPictureSpeed * Exelius::Time::DeltaTime.GetAsSeconds());
		else if (Exelius::IsKeyDown(Exelius::KeyCode::D))
			transform->SetX(transform->GetX() + kPictureSpeed * Exelius::Time::DeltaTime.GetAsSeconds());

		if (transform->GetX() < 0.0f)
			transform->SetX(0.0f);

		if (transform->GetX() > 384.0f)
			transform->SetX(384.0f);
	}
}

void UIResizeApp::StretchImage()
{
	auto* pGameObjectSystem = Exelius::GameObjectSystem::GetInstance();
	EXE_ASSERT(pGameObjectSystem);

	const auto& pObject = pGameObjectSystem->GetGameObject(m_activeImage);
	if (auto ui = pObject->GetComponent<Exelius::UIComponent>())
	{
		if (Exelius::IsKeyDown(Exelius::KeyCode::Up))
			ui->SetH(ui->GetH() - 100.0f * Exelius::Time::DeltaTime.GetAsSeconds());
		else if (Exelius::IsKeyDown(Exelius::KeyCode::Down))
			ui->SetH(ui->GetH() + 100.0f * Exelius::Time::DeltaTime.GetAsSeconds());

		if (ui->GetH() < 1.0f)
			ui->SetH(1.0f);

		if (ui->GetH() > 384.0f)
			ui->SetH(384.0f);


		if (Exelius::IsKeyDown(Exelius::KeyCode::Left))
			ui->SetW(ui->GetW() - 100.0f * Exelius::Time::DeltaTime.GetAsSeconds());
		else if (Exelius::IsKeyDown(Exelius::KeyCode::Right))
			ui->SetW(ui->GetW() + 100.0f * Exelius::Time::DeltaTime.GetAsSeconds());

		if (ui->GetW() < 1.0f)
			ui->SetW(1.0f);

		if (ui->GetW() > 384.0f)
			ui->SetW(384.0f);
	}
}
