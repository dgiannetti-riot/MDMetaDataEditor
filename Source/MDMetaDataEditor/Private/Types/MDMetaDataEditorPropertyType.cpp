// Copyright Dylan Dumesnil. All Rights Reserved.


#include "MDMetaDataEditorPropertyType.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "InstancedStruct.h"

FEdGraphPinType FMDMetaDataEditorPropertyType::ToGraphPinType() const
{
	FEdGraphPinType PinType;
	PinType.PinCategory = PropertyType;
	PinType.PinSubCategory = PropertySubType;
	PinType.PinSubCategoryObject = PropertySubTypeObject.LoadSynchronous();
	PinType.PinSubCategoryMemberReference = PropertySubTypeMemberReference;

	if (ValueType.IsValid())
	{
		PinType.PinValueType = ValueType.Get<FMDMetaDataEditorPropertyType>().ToGraphTerminalType();
	}

	switch (ContainerType)
	{
	case EMDMetaDataPropertyContainerType::None:
		PinType.ContainerType = EPinContainerType::None;
		break;
	case EMDMetaDataPropertyContainerType::Array:
		PinType.ContainerType = EPinContainerType::Array;
		break;
	case EMDMetaDataPropertyContainerType::Set:
		PinType.ContainerType = EPinContainerType::Set;
		break;
	case EMDMetaDataPropertyContainerType::Map:
		PinType.ContainerType = EPinContainerType::Map;
		break;
	}

	return PinType;
}

FEdGraphTerminalType FMDMetaDataEditorPropertyType::ToGraphTerminalType() const
{
	FEdGraphTerminalType TerminalType;
	TerminalType.TerminalCategory = PropertyType;
	TerminalType.TerminalSubCategory = PropertySubType;
	TerminalType.TerminalSubCategoryObject = PropertySubTypeObject.LoadSynchronous();

	return TerminalType;
}

void FMDMetaDataEditorPropertyType::SetFromGraphPinType(const FEdGraphPinType& GraphPinType)
{
	PropertyType = GraphPinType.PinCategory;
	PropertySubType = GraphPinType.PinSubCategory;
	PropertySubTypeObject = GraphPinType.PinSubCategoryObject.Get();
	PropertySubTypeMemberReference = GraphPinType.PinSubCategoryMemberReference;

	if (!GraphPinType.PinValueType.TerminalCategory.IsNone())
	{
		ValueType = ValueType.Make<FMDMetaDataEditorPropertyType>();
		ValueType.GetMutable<FMDMetaDataEditorPropertyType>().SetFromGraphTerminalType(GraphPinType.PinValueType);
	}
	else if (GraphPinType.ContainerType == EPinContainerType::Map)
	{
		ValueType = ValueType.Make<FMDMetaDataEditorPropertyType>();
	}
	else
	{
		ValueType.Reset();
	}

	switch (GraphPinType.ContainerType)
	{
	case EPinContainerType::None:
		ContainerType = EMDMetaDataPropertyContainerType::None;
		break;
	case EPinContainerType::Array:
		ContainerType = EMDMetaDataPropertyContainerType::Array;
		break;
	case EPinContainerType::Set:
		ContainerType = EMDMetaDataPropertyContainerType::Set;
		break;
	case EPinContainerType::Map:
		ContainerType = EMDMetaDataPropertyContainerType::Map;
		break;
	}
}

void FMDMetaDataEditorPropertyType::SetFromGraphTerminalType(const FEdGraphTerminalType& GraphTerminalType)
{
	PropertyType = GraphTerminalType.TerminalCategory;
	PropertySubType = GraphTerminalType.TerminalSubCategory;
	PropertySubTypeObject = GraphTerminalType.TerminalSubCategoryObject.Get();
}

bool FMDMetaDataEditorPropertyType::DoesMatchProperty(const FProperty* Property) const
{
	if (Property == nullptr)
	{
		return false;
	}

	const FProperty* EffectiveProp = Property;

	if (ContainerType == EMDMetaDataPropertyContainerType::Array && !Property->IsA<FArrayProperty>())
	{
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
		if (ArrayProperty == nullptr)
		{
			return false;
		}

		EffectiveProp = ArrayProperty->Inner;
	}
	else if (ContainerType == EMDMetaDataPropertyContainerType::Set && !Property->IsA<FSetProperty>())
	{
		const FSetProperty* SetProperty = CastField<FSetProperty>(Property);
		if (SetProperty == nullptr)
		{
			return false;
		}

		EffectiveProp = SetProperty->ElementProp;
	}
	else if (ContainerType == EMDMetaDataPropertyContainerType::Map)
	{
		const FMapProperty* MapProperty = CastField<FMapProperty>(Property);
		if (MapProperty == nullptr)
		{
			return false;
		}

		const FMDMetaDataEditorPropertyType* ValueTypePtr = ValueType.GetPtr<FMDMetaDataEditorPropertyType>();
		if (ValueTypePtr == nullptr || !ValueTypePtr->DoesMatchProperty(MapProperty->ValueProp))
		{
			return false;
		}

		EffectiveProp = MapProperty->KeyProp;
	}

	if (PropertyType == UEdGraphSchema_K2::PC_Wildcard)
	{
		return true;
	}

	if (PropertyType == UEdGraphSchema_K2::PC_Struct)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty == nullptr || StructProperty->Struct == nullptr)
		{
			return false;
		}

		if (PropertySubTypeObject.IsNull() || StructProperty->Struct->IsChildOf(Cast<UStruct>(PropertySubTypeObject.Get())))
		{
			return true;
		}
	}

	if (PropertyType == UEdGraphSchema_K2::PC_Object || PropertyType == UEdGraphSchema_K2::PC_SoftObject)
	{
		const FObjectPropertyBase* ObjectProperty = (PropertyType == UEdGraphSchema_K2::PC_Object)
			? CastField<FObjectPropertyBase>(Property)
			: CastField<FSoftObjectProperty>(Property);

		if (ObjectProperty == nullptr || ObjectProperty->PropertyClass == nullptr)
		{
			return false;
		}

		if (PropertySubTypeObject.IsNull() || ObjectProperty->PropertyClass->IsChildOf(Cast<UClass>(PropertySubTypeObject.Get())))
		{
			return true;
		}
	}

	if (PropertyType == UEdGraphSchema_K2::PC_Class || PropertyType == UEdGraphSchema_K2::PC_SoftClass)
	{
		if (!Property->IsA<FClassProperty>() && !Property->IsA<FSoftClassProperty>())
		{
			return false;
		}

		const UClass* MetaClass = (PropertyType == UEdGraphSchema_K2::PC_Class)
			? CastField<FClassProperty>(Property)->MetaClass
			: CastField<FSoftClassProperty>(Property)->MetaClass;
		if (MetaClass == nullptr)
		{
			return false;
		}

		if (PropertySubTypeObject.IsNull() || MetaClass->IsChildOf(Cast<UClass>(PropertySubTypeObject.Get())))
		{
			return true;
		}
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;
	if (!K2Schema->ConvertPropertyToPinType(EffectiveProp, PinType))
	{
		return false;
	}

	return PropertyType == PinType.PinCategory
		&& PropertySubType == PinType.PinSubCategory
		&& PropertySubTypeObject.Get() == PinType.PinSubCategoryObject
		&& PropertySubTypeMemberReference == PinType.PinSubCategoryMemberReference;
}

bool FMDMetaDataEditorPropertyType::operator==(const FMDMetaDataEditorPropertyType& Other) const
{
	return PropertyType == Other.PropertyType
		&& PropertySubType == Other.PropertySubType
		&& PropertySubTypeObject == Other.PropertySubTypeObject
		&& PropertySubTypeMemberReference == Other.PropertySubTypeMemberReference
		&& ValueType == Other.ValueType
		&& ContainerType == Other.ContainerType;
}
