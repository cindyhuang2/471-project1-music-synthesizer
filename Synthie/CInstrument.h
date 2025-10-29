#pragma once
#include "CAudioNode.h"
#include "CNote.h"

class CInstrument : public CAudioNode
{
public:
	CInstrument();
	virtual ~CInstrument();

	virtual void SetNote(CNote* note) = 0;
};

