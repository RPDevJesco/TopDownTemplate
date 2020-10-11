#include "StdAfx.h"
#include "Player.h"
#include "Bullet.h"
#include "GamePlugin.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryInput/IHardwareMouse.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>

// Namespace to store our registration component which is needed for our class to show up in the editor.
namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

void CPlayerComponent::Initialize()
{
	// The character controller is responsible for maintaining player physics
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	// Offset the default character controller up by one unit
	m_pCharacterController->SetTransformMatrix(Matrix34::Create(Vec3(1.f), IDENTITY, Vec3(0, 0, 1.f)));

	// Create the advanced animation component, responsible for updating Mannequin and animating the player
	m_pAnimationComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CAdvancedAnimationComponent>();
	
	// Set the player geometry, this also triggers physics proxy creation
	m_pAnimationComponent->SetMannequinAnimationDatabaseFile("Animations/Mannequin/ADB/FirstPerson.adb");
	// Set the player geometry based on the model stored in the engine.
	m_pAnimationComponent->SetCharacterFile("Objects/Characters/SampleCharacter/thirdperson.cdf");
	// Controller Definition sets our blend spaces and animations that are available for our model.
	m_pAnimationComponent->SetControllerDefinitionFile("Animations/Mannequin/ADB/FirstPersonControllerDefinition.xml");
	// Sets the context name based off what is set in the blendspace editor / xml file.
	m_pAnimationComponent->SetDefaultScopeContextName("FirstPersonCharacter");
	// Queue the idle fragment to start playing immediately on next update
	m_pAnimationComponent->SetDefaultFragmentName("Idle");

	// Disable movement coming from the animation (root joint offset), we control this entirely via physics
	m_pAnimationComponent->SetAnimationDrivenMotion(true);
	// Sets the model, animation and physics to align with the ground.
	m_pAnimationComponent->EnableGroundAlignment(true);
	// Load the character and Mannequin data from file
	m_pAnimationComponent->LoadFromDisk();

	// Acquire tag identifiers to avoid doing so each update
	m_walkTagId = m_pAnimationComponent->GetTagId("Walk");
	// Initializes the remaining items we need.
	InitializePlayer();
}

void CPlayerComponent::InitializePlayer()
{
	// Create the camera component, will automatically update the viewport every frame
	m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();

	// Create the audio listener component.
	m_pAudioListenerComponent = m_pEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();
	
	// Get the input component, wraps access to action mapping so we can easily get callbacks when inputs are triggered
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
	
	// Register an action, and the callback that will be sent when it's triggered
	m_pInputComponent->RegisterAction("player", "moveleft", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveLeft, (EActionActivationMode)activationMode);  }); 
	// Bind the 'A' key the "moveleft" action and so on.
	m_pInputComponent->BindAction("player", "moveleft", eAID_KeyboardMouse,	EKeyId::eKI_A);
	m_pInputComponent->RegisterAction("player", "moveright", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveRight, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);
	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);
	m_pInputComponent->RegisterAction("player", "moveback", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveBack, (EActionActivationMode)activationMode);  }); 
	m_pInputComponent->BindAction("player", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);
	m_pInputComponent->RegisterAction("player", "mouse_rotateyaw", [this](int activationMode, float value) { m_mouseDeltaRotation.x -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);
	// Register and bind for mouse movement, the callback will be sent when triggered.
	m_pInputComponent->RegisterAction("player", "mouse_rotatepitch", [this](int activationMode, float value) { m_mouseDeltaRotation.y -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);
	
	// Register the shoot action
	m_pInputComponent->RegisterAction("player", "shoot", [this](int activationMode, float value)
	{
		// Only fire on press, not release
		if (activationMode == eAAM_OnPress)
		{
			// Grabbing our character and the rifle attachments.
			if (ICharacterInstance* pCharacter = m_pAnimationComponent->GetCharacter())
			{
				IAttachment* pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");

				if (pBarrelOutAttachment != nullptr)
				{
					QuatTS bulletOrigin = pBarrelOutAttachment->GetAttWorldAbsolute();
					// Setting our spawn params for the bullet that will be fired.
					SEntitySpawnParams spawnParams;
					spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

					spawnParams.vPosition = bulletOrigin.t;
					spawnParams.qRotation = bulletOrigin.q;

					const float bulletScale = 0.05f;
					spawnParams.vScale = Vec3(bulletScale);

					// Spawn the entity
					if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
					{
						// See Bullet.h, bullet is propelled in the rotation and position the entity was spawned with
						pEntity->CreateComponentClass<CBulletComponent>();
					}
				}
			}
		}
	});

	// Bind the shoot action to left mouse click
	m_pInputComponent->BindAction("player", "shoot", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);
	// Spawn the cursor
	SpawnCursorEntity();
}

// Processing which Event flags we need.
Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return
		Cry::Entity::EEvent::Initialize |
		Cry::Entity::EEvent::GameplayStarted |
		Cry::Entity::EEvent::Update |
		Cry::Entity::EEvent::Reset;
}

// We handle each of the event flags we need.
void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		// Initialization step will call both of our initialization functions and set our alive check to true.
	case Cry::Entity::EEvent::Initialize:
	{
		m_isAlive = true;
		Initialize();
	}
	break;
	// Game play handling for when it has just started, we want to reset the player position, animation and input flags.
	case Cry::Entity::EEvent::GameplayStarted:
	{
		ResetPlayer();
	}
	break;
	case Cry::Entity::EEvent::Update:
	{
			// Don't update the player if we haven't spawned yet
			if (!m_isAlive)
				return;
			// C++ version of implementing / creating frame time in CE.
			const float frameTime = event.fParam[0];

			// Update the in-world cursor position
			UpdateCursor(frameTime);

			// Start by updating the movement request we want to send to the character controller
			// This results in the physical representation of the character moving
			UpdateMovementRequest(frameTime);

			// Update the animation state of the character
			UpdateAnimation(frameTime);

			// Update the camera component offset
			UpdateCamera(frameTime);
	}
	break;
	// While not actively used, it is good to go ahead and implement a reset event for when the player dies,
	// level is restarted and so on.
	case Cry::Entity::EEvent::Reset:
	{
		ResetPlayer();
	}
	break;
	}
}

void CPlayerComponent::SpawnCursorEntity()
{
	if (m_pCursorEntity)
	{
		gEnv->pEntitySystem->RemoveEntity(m_pCursorEntity->GetId());
	}

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

	// Spawn the cursor
	m_pCursorEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

	// Load geometry for the cursor, in our case, it is just a sphere.
	const int geometrySlot = 0;
	m_pCursorEntity->LoadGeometry(geometrySlot, "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");

	// Scale the cursor down a bit
	m_pCursorEntity->SetScale(Vec3(0.1f));
	m_pCursorEntity->SetViewDistRatio(255);

	// Load the custom cursor material
	IMaterial* pCursorMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Materials/cursor");
	m_pCursorEntity->SetMaterial(pCursorMaterial);
}

void CPlayerComponent::UpdateMovementRequest(float frameTime)
{
	// Don't handle input if we are in air
	if (!m_pCharacterController->IsOnGround())
		return;
	// initialize our velocity variable to be a zero vector
	Vec3 velocity = ZERO;
	// create a move speed float value, 20.5 is a smooth movement speed.
	const float moveSpeed = 20.5f;
	// Utilizing our input flags, we can manipulate how the player moves.
	if (m_inputFlags & EInputFlag::MoveLeft)
	{
		velocity.x -= moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveRight)
	{
		velocity.x += moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveForward)
	{
		velocity.y += moveSpeed * frameTime;
	}
	if (m_inputFlags & EInputFlag::MoveBack)
	{
		velocity.y -= moveSpeed * frameTime;
	}
	// update the character controller's velocity based off the velocity value as it changes.
	m_pCharacterController->AddVelocity(velocity);
}

void CPlayerComponent::UpdateAnimation(float frameTime)
{
	// Update the Mannequin tags
	m_pAnimationComponent->SetTagWithId(m_walkTagId, true);
	// if the cursor is null, don't update the animation.
	if (m_pCursorEntity == nullptr)
	{
		return;
	}
	// Dir is a direction vector 3 value, it will be the difference between the cursor's world position and 
	// the player character' world position.
	Vec3 dir = m_pCursorEntity->GetWorldPos() - m_pEntity->GetWorldPos();
	// normalize the results.
	dir = dir.Normalize();
	// newRotation is a Quaternion which will be a rotation v direction
	// which is based off the direction vector we created.
	Quat newRotation = Quat::CreateRotationVDir(dir);
	// ypr is Yaw Pit Rotation from Angles3. It will be created from the Camera Components'
	// function to create angles YPR. The parameter is a 3x3 matrix based off our newRotation variable.
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(newRotation));

	// We only want to affect Z-axis rotation, zero pitch and roll
	ypr.y = 0;
	ypr.z = 0;

	// Re-calculate the quaternion based on the corrected yaw
	newRotation = Quat(CCamera::CreateOrientationYPR(ypr));

	// If the character controller is walking
	if (m_pCharacterController->IsWalking())
	{
		// Send updated transform to the entity, only orientation changes
		m_pEntity->SetPosRotScale(m_pEntity->GetWorldPos(), newRotation, Vec3(1, 1, 1));
	}
	// If the character controller is not walking
	else
	{
		// Update only the player rotation
		m_pEntity->SetRotation(newRotation);
	}
}

void CPlayerComponent::UpdateCamera(float frameTime)
{
	// Start with rotating the camera to face downwards
	Matrix34 localTransform = IDENTITY;
	localTransform.SetRotation33(Matrix33(m_pEntity->GetWorldRotation().GetInverted()) * Matrix33::CreateRotationX(DEG2RAD(-90)));

	// change this to have fun results of the camera's distance from the character.
	const float viewDistanceFromPlayer = 10.f;

	// Offset the player along the forward axis (normally back)
	// Also offset upwards. This affects the camera and the audio components.
	localTransform.SetTranslation(Vec3(0, 0, viewDistanceFromPlayer));
	m_pCameraComponent->SetTransformMatrix(localTransform);
	m_pAudioListenerComponent->SetOffset(localTransform.GetTranslation());
}

void CPlayerComponent::UpdateCursor(float frameTime)
{
	float mouseX, mouseY;
	// Get the mouse's position.
	gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);

	// Invert mouse Y
	mouseY = gEnv->pRenderer->GetHeight() - mouseY;

	Vec3 vPos0(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 0, &vPos0.x, &vPos0.y, &vPos0.z);

	Vec3 vPos1(0, 0, 0);
	gEnv->pRenderer->UnProjectFromScreen(mouseX, mouseY, 1, &vPos1.x, &vPos1.y, &vPos1.z);

	// Grab the difference between the results of the vPos0 and vPos1's mouse position and unprojected
	// results from the screen.
	Vec3 vDir = vPos1 - vPos0;
	// normalize the results.
	vDir.Normalize();

	// Raycasting time
	const unsigned int rayFlags = rwi_stop_at_pierceable | rwi_colltype_any;
	ray_hit hit;

	// Read the function definition of RayWorldInsersection to get what it does as
	// I would just be repeating it if documenting it out here. https://docs.cryengine.com//pages/viewpage.action?pageId=29797387
	if (gEnv->pPhysicalWorld->RayWorldIntersection(vPos0, vDir * gEnv->p3DEngine->GetMaxViewDistance(), ent_all, rayFlags, &hit, 1))
	{
		m_cursorPositionInWorld = hit.pt;

		if (m_pCursorEntity != nullptr)
		{
			m_pCursorEntity->SetPosRotScale(hit.pt, IDENTITY, m_pCursorEntity->GetScale());
		}
	}
	else
	{
		m_cursorPositionInWorld = ZERO;
	}
}

void CPlayerComponent::ResetPlayer()
{
	// Apply character to the entity
	m_pAnimationComponent->ResetCharacter();
	m_pCharacterController->Physicalize();
	// Reset input now that the player respawned
	m_inputFlags.Clear();
}

void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode, const EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
	{
		if (activationMode == eAAM_OnRelease)
		{
			m_inputFlags &= ~flags;
		}
		else
		{
			m_inputFlags |= flags;
		}
	}
	break;
	case EInputFlagType::Toggle:
	{
		if (activationMode == eAAM_OnRelease)
		{
			// Toggle the bit(s)
			m_inputFlags ^= flags;
		}
	}
	break;
	}
}