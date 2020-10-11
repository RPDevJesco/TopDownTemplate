#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>

////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
	// Creating an FlagType enum for storing if a button has been held or toggled.
	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	// Creating an InputFlag for handling movement
	enum class EInputFlag : uint8
	{
		// Note we are bit shifting to the left here.
		// Move left's value is 1U and we are shifting 1 left which points to 0 which makes the value 10.
		MoveLeft = 1 << 0,
		MoveRight = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack = 1 << 3
	};
// The below functions are public and are engine defined.
public:
	// Constructor for CPlayerComponent is the default constructor provided by Crytek.
	CPlayerComponent() = default;
	// Destructor for CPlayerComponent is the default destructor provided by Crytek.
	virtual ~CPlayerComponent() = default;

	// We need the initialization function and we will override it to suit our purposes.
	virtual void Initialize() override;
	// We need the GetEventMask function and we will override it to suit our purposes.
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	// We need the ProcessEvent function and we will override it to suit our purposes.
	virtual void ProcessEvent(const SEntityEvent& event) override;

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
	}
	
	// The below functions are private and are self defined.
protected:
	// We need a request for updating movement and will require the frameTime value in the parameter.
	void UpdateMovementRequest(float frameTime);
	// We need a request for updating animation and will require the frameTime value in the parameter.
	void UpdateAnimation(float frameTime);
	// We need a request for updating the camera and will require the frameTime value in the parameter.
	void UpdateCamera(float frameTime);
	// We need a request for updating the cursor and will require the frameTime value in the parameter.
	void UpdateCursor(float frameTime);
	// We need to actually spawn our cursor.
	void SpawnCursorEntity();
	// We need to initialize the player and will be called in the default initialize function that we overrided.
	void InitializePlayer();
	// We need to be able to reset the position, animation and input flags which will be handled here.
	void ResetPlayer();
	// We need a function to actually handle our input flag changes. Will be done here.
	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);
// The below properties are private..
protected:
	// a boolean value to track if the player is alive or dead.
	bool m_isAlive = false;
	// Definining of our camera component variable and instantiating it as null.
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	// Definining of our character controller component variable and instantiating it as null.
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController = nullptr;
	// Definining of our advanced animation component variable and instantiating it as null.
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAnimationComponent = nullptr;
	// Definining of our input component variable and instantiating it as null.
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;
	// Definining of our audio listener component variable and instantiating it as null.
	Cry::Audio::DefaultComponents::CListenerComponent* m_pAudioListenerComponent = nullptr;
	// Defining of a TagID which is needed for the advanced animation component.
	TagID m_walkTagId;
	// Definining of our input flags to be able to handle player movement.
	CEnumFlags<EInputFlag> m_inputFlags;
	// Definining of our Vector2 which will store the mouse rotation.
	Vec2 m_mouseDeltaRotation;
	// Definining of our Vector2 which will store the mouse position and initializing it to zero vector position.
	Vec3 m_cursorPositionInWorld = ZERO;
	// Definining of our mouse cursor and initializing it as a null pointer.
	IEntity* m_pCursorEntity = nullptr;
};
