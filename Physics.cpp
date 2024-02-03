struct collision
{
    bool DidCollide;
    v2 Normal;
    f32 Penetration;
};

static inline rect
RectOf(rigid_body RigidBody)
{
    v2 MinCorner = RigidBody.P - 0.5f * RigidBody.Size;
    v2 MaxCorner = RigidBody.P + 0.5f * RigidBody.Size;
    return {MinCorner, MaxCorner};
}

static collision
TestAABBCollision(rect RectA, rect RectB)
{
    collision Result = {};
    
    if (RectanglesCollide(RectA, RectB))
    {
        v2 Sides[] = {
            V2(-1, 0),
            V2(1, 0),
            V2(0, -1),
            V2(0, 1)
        };
        
        f32 Distances[] = {
            RectB.MaxCorner.X - RectA.MinCorner.X,
            RectA.MaxCorner.X - RectB.MinCorner.X,
            RectB.MaxCorner.Y - RectA.MinCorner.Y,
            RectA.MaxCorner.Y - RectB.MinCorner.Y
        };
        
        f32 Penetration = 1000.0f;
        v2 Normal = {};
        for (int Index = 0; Index < 4; Index++)
        {
            if (Distances[Index] < Penetration)
            {
                Penetration = Distances[Index];
                Normal = Sides[Index];
            }
        }
        
        Result.DidCollide = true;
        Result.Normal = Normal;
        Result.Penetration = Penetration;
    }
    return Result;
}



static void
PhysicsUpdate(span<rigid_body> RigidBodies, f32 DeltaTime, v2 Movement, rigid_body* Controlling)
{
    v2 Gravity = V2(0.0f, -2.5f);
    
    int PhysicsIterPerFrame = 3;
    f32 TimeStep = DeltaTime / (f32)PhysicsIterPerFrame;
    for (int Iter = 0; Iter < PhysicsIterPerFrame; Iter++)
    {
        for (rigid_body& RigidBody : RigidBodies)
        {
            f32 Friction = 10.0f;
            
            v2 Accel = Gravity - Friction * RigidBody.dP;
            if (&RigidBody == Controlling)
            {
                f32 Speed = 5.0f;
                Accel += Speed * Movement;
            }
            
            v2 ddP = Accel * RigidBody.InvMass;
            
            RigidBody.P += TimeStep * RigidBody.dP + 0.5f * ddP * Square(TimeStep);
            RigidBody.dP += TimeStep * ddP;
        }
        
        for (u32 IndexA = 0; IndexA < RigidBodies.Count; IndexA++)
        {
            rigid_body* A = RigidBodies + IndexA;
            for (u32 IndexB = 0; IndexB < IndexA; IndexB++)
            {
                rigid_body* B = RigidBodies + IndexB;
                
                collision Collision = TestAABBCollision(RectOf(*A), RectOf(*B));
                if (Collision.DidCollide)
                {
                    f32 SumInverseMass = A->InvMass + B->InvMass;
                    
                    if (SumInverseMass == 0.0f)
                    {
                        continue;
                    }
                    
                    A->P -= Collision.Normal * Collision.Penetration * (A->InvMass / SumInverseMass);
                    B->P += Collision.Normal * Collision.Penetration * (B->InvMass / SumInverseMass);
                    
                    v2 VelContact = B->dP - A->dP;
                    f32 CoefficientOfRestitution = 0.0f;
                    
                    f32 ImpulseForce = DotProduct(VelContact, Collision.Normal);
                    
                    f32 j = (-(1.0f + CoefficientOfRestitution) * ImpulseForce) / (SumInverseMass);
                    
                    v2 Impulse = Collision.Normal * j;
                    
                    A->dP -= Impulse * A->InvMass;
                    B->dP += Impulse * B->InvMass;
                }
                
            }
        }
        
    }
}
