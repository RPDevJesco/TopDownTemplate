#pragma once
////////////////////////////////////////////////////////
// Physicalized bullet shot from weaponry, expires x seconds after collision with another object
////////////////////////////////////////////////////////
class CBulletComponent final : public IEntityComponent
{
public:
	// Destructor for the bullet component.
	virtual ~CBulletComponent() {}
	// implement the initialize function here.
	virtual void Initialize() override
	{
		// Set the model
		const int geometrySlot = 0;
		m_pEntity->LoadGeometry(geometrySlot, "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");

		// Load the custom bullet material.
		// This material has the 'mat_bullet' surface type applied, which is set up to play sounds on collision with 'mat_default' objects in Libs/MaterialEffects
		auto* pBulletMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Materials/bullet");
		m_pEntity->SetMaterial(pBulletMaterial);

		// Now create the physical representation of the entity
		SEntityPhysicalizeParams physParams;
		// Rigid physicalization type
		physParams.type = PE_RIGID;
		// We have a mass value based on arbitrary values. We don't want it to throw the world around,
		// but we also don't want it to be super weak either.
		physParams.mass = 20000.f;
		m_pEntity->Physicalize(physParams);

		// Make sure that bullets are always rendered regardless of distance
		// Ratio is 0 - 255, 255 being 100% visibility
		GetEntity()->SetViewDistRatio(255);

		// Apply an impulse so that the bullet flies forward
		if (auto* pPhysics = GetEntity()->GetPhysics())
		{
			pe_action_impulse impulseAction;
			// Change this velocity value for some real fun.
			const float initialVelocity = 1000.f;

			// Set the actual impulse, in this cause the value of the initial velocity CVar in bullet's forward direction
			impulseAction.impulse = GetEntity()->GetWorldRotation().GetColumn1() * initialVelocity;

			// Send to the physical entity
			pPhysics->Action(&impulseAction);
		}
	}

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CBulletComponent>& desc)
	{
		desc.SetGUID("{FECA6E51-D1AD-478D-AD17-BACD6D712609}"_cry_guid);
	}

	// Grabbing the event flags we need.
	virtual Cry::Entity::EventFlags GetEventMask() const override
	{
		return
			Cry::Entity::EEvent::Update |
			ENTITY_EVENT_COLLISION;
	}
	// Handling our event flags.
	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		// this event is triggered when collision occurs.
		if (event.event == ENTITY_EVENT_COLLISION)
		{
			// Do a check if the timer is less than 0
			if (m_timer < 0)
			{
				// If the timer is less than zero, remove the bullet from the scene.
				gEnv->pEntitySystem->RemoveEntity(GetEntityId());
			}
		}
		// this event is triggered on every update call.
		if (event.event == Cry::Entity::EEvent::Update)
		{
			// set our frametime to be the actual frametime from the last update call.
			float frameTime = gEnv->pTimer->GetFrameTime();
			// clamp our timer values. We count down from the timer value based on the frametime.
			// We clamp the max value to be 1 and minimum of -1.
			m_timer = CLAMP(m_timer - frameTime, -1.f, 1.f);
		}
	}
// Private variables here.
private:
	// A timer for how long it would take before the bullet "dies".
	float m_timer = 5;
};